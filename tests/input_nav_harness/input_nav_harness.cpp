#include <nlohmann/json.hpp>

#include "lvgl.h"
#include "seedsigner.h"
#include "input_profile.h"

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::ordered_json;

typedef void (*screen_fn_t)(void *ctx_json);

static const std::unordered_map<std::string, screen_fn_t> k_screen_registry = {
    {"button_list_screen", button_list_screen},
    {"main_menu_screen", main_menu_screen},
};

static std::vector<std::string> g_events;
static uint32_t g_pending_key = 0;
static bool g_key_ready = false;
static bool g_key_released_once = true;

extern "C" void seedsigner_lvgl_on_button_selected(uint32_t index, const char *label) {
    (void)index;
    g_events.push_back(label ? std::string(label) : std::string(""));
}

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    (void)area;
    (void)color_p;
    lv_disp_flush_ready(drv);
}

static void keypad_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    if (g_key_ready) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = g_pending_key;
        g_key_ready = false;
        g_key_released_once = false;
        return;
    }

    if (!g_key_released_once) {
        data->state = LV_INDEV_STATE_RELEASED;
        g_key_released_once = true;
        return;
    }

    data->state = LV_INDEV_STATE_RELEASED;
}

static uint32_t parse_key(const std::string &k) {
    if (k == "UP") return LV_KEY_UP;
    if (k == "DOWN") return LV_KEY_DOWN;
    if (k == "LEFT") return LV_KEY_LEFT;
    if (k == "RIGHT") return LV_KEY_RIGHT;
    if (k == "ENTER") return LV_KEY_ENTER;
    if (k == "1") return '1';
    if (k == "2") return '2';
    if (k == "3") return '3';
    return 0;
}

static lv_obj_t *find_first_grouped_button(lv_obj_t *root) {
    if (!root) return NULL;
    if (lv_obj_check_type(root, &lv_btn_class) && lv_obj_get_group(root) != NULL) return root;

    uint32_t n = lv_obj_get_child_cnt(root);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t *ch = lv_obj_get_child(root, i);
        lv_obj_t *found = find_first_grouped_button(ch);
        if (found) return found;
    }
    return NULL;
}

static int run_case(const json &c) {
    std::string name = c.value("name", "unnamed");
    std::string screen = c.value("screen", "");
    if (screen.empty()) {
        std::cerr << "[FAIL] " << name << " missing screen\n";
        return 1;
    }

    auto it = k_screen_registry.find(screen);
    if (it == k_screen_registry.end()) {
        std::cerr << "[FAIL] " << name << " unknown screen: " << screen << "\n";
        return 1;
    }

    g_events.clear();
    json ctx = c.value("context", json::object());
    std::string ctx_json = ctx.empty() ? std::string() : ctx.dump();
    it->second(ctx_json.empty() ? nullptr : (void *)ctx_json.c_str());

    lv_timer_handler();
    lv_tick_inc(10);

    lv_obj_t *first_btn = find_first_grouped_button(lv_scr_act());
    lv_group_t *group = first_btn ? (lv_group_t *)lv_obj_get_group(first_btn) : NULL;
    if (!group) {
        std::cerr << "[FAIL] " << name << " unable to resolve LVGL group from screen\n";
        return 1;
    }

    for (const auto &kj : c["keys"]) {
        std::string ks = kj.get<std::string>();
        uint32_t key = parse_key(ks);
        if (key == 0) {
            std::cerr << "[FAIL] " << name << " unknown key: " << ks << "\n";
            return 1;
        }

        lv_group_send_data(group, key);

        // Pump cycles for state transitions/event callbacks.
        for (int i = 0; i < 2; ++i) {
            lv_tick_inc(10);
            lv_timer_handler();
        }
    }

    std::vector<std::string> expected;
    for (const auto &e : c["expect_events"]) expected.push_back(e.get<std::string>());

    if (g_events != expected) {
        std::cerr << "[FAIL] " << name << "\n  expected:";
        for (const auto &e : expected) std::cerr << " [" << e << "]";
        std::cerr << "\n  actual:";
        for (const auto &e : g_events) std::cerr << " [" << e << "]";
        std::cerr << "\n";
        return 1;
    }

    std::cout << "[PASS] " << name << "\n";
    return 0;
}

int main(int argc, char **argv) {
    const char *cases_path = "tests/input_nav_harness/cases/basic_nav_cases.json";
    if (argc > 1) cases_path = argv[1];

    std::ifstream ifs(cases_path);
    if (!ifs.is_open()) {
        std::cerr << "Cannot open cases file: " << cases_path << "\n";
        return 2;
    }

    json root;
    ifs >> root;
    if (!root.is_object() || !root.contains("cases") || !root["cases"].is_array()) {
        std::cerr << "Invalid cases file format\n";
        return 2;
    }

    lv_init();
    static std::vector<lv_color_t> draw_buf(480 * 40);
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, draw_buf.data(), NULL, (uint32_t)draw_buf.size());

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 480;
    disp_drv.ver_res = 320;
    disp_drv.flush_cb = flush_cb;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = keypad_read_cb;
    lv_indev_drv_register(&indev_drv);

    input_profile_set_mode(INPUT_MODE_HARDWARE);

    int fails = 0;
    for (const auto &c : root["cases"]) {
        fails += run_case(c);
    }

    if (fails == 0) {
        std::cout << "All cases passed\n";
        return 0;
    }

    std::cerr << fails << " case(s) failed\n";
    return 1;
}
