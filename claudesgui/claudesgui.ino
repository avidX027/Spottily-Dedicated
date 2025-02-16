#include <TFT_eSPI.h>
#include <lvgl.h>
#include <Arduino.h>
#include "TouchDrvGT911.hpp"
#include "utilities.h"
#include <Wire.h>

#define LILYGO_KB_SLAVE_ADDRESS     0x55

// GUI Elements
static lv_obj_t *now_playing_label;
static lv_obj_t *artist_label;
static lv_obj_t *progress_slider;
static lv_obj_t *progress_label;
static lv_obj_t *shuffle_btn;
static lv_obj_t *like_btn;

// Style for the control buttons
static lv_style_t style_btn;

TFT_eSPI tft;
TouchDrvGT911 touch;

// Function declarations
static void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
static void touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);
static void slider_event_cb(lv_event_t *e);
static void control_btn_event_cb(lv_event_t *e);

static void init_styles() {
    lv_style_init(&style_btn);
    lv_style_set_radius(&style_btn, 10);
    lv_style_set_bg_color(&style_btn, lv_color_hex(0x00fc37));
    lv_style_set_text_color(&style_btn, lv_color_white());
    lv_style_set_pad_all(&style_btn, 8);
}

static void create_spotify_gui() {
    // Create main container
    lv_obj_t *main_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_cont, 320, 240);
    lv_obj_set_style_pad_all(main_cont, 10, 0);
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0x121212), 0);
    lv_obj_set_style_text_color(main_cont, lv_color_white(), 0);
    
    // Now Playing section
    now_playing_label = lv_label_create(main_cont);
    lv_obj_align(now_playing_label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(now_playing_label, "Now Playing: No Track");
    
    artist_label = lv_label_create(main_cont);
    lv_obj_align(artist_label, LV_ALIGN_TOP_LEFT, 0, 25);
    lv_label_set_text(artist_label, "Artist: None");
    
    // Progress slider with updated styling
    progress_slider = lv_slider_create(main_cont);
    lv_obj_set_width(progress_slider, 280);
    lv_obj_align(progress_slider, LV_ALIGN_TOP_MID, 0, 60);
    
    lv_obj_set_style_bg_color(progress_slider, lv_color_hex(0x535353), LV_PART_MAIN);
    lv_obj_set_style_bg_color(progress_slider, lv_color_hex(0x00fc37), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(progress_slider, lv_color_hex(0x00fc37), LV_PART_KNOB);
    
    lv_obj_add_event_cb(progress_slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    progress_label = lv_label_create(main_cont);
    lv_obj_align(progress_label, LV_ALIGN_TOP_MID, 0, 85);
    lv_label_set_text(progress_label, "0:00 / 0:00");
    
    // Control buttons container
    lv_obj_t *controls_cont = lv_obj_create(main_cont);
    lv_obj_set_size(controls_cont, 300, 50);
    lv_obj_align(controls_cont, LV_ALIGN_TOP_MID, 0, 110);
    lv_obj_set_flex_flow(controls_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(controls_cont, 0, 0);
    
    // Previous button
    lv_obj_t *prev_btn = lv_btn_create(controls_cont);
    lv_obj_set_size(prev_btn, 50, 40);
    lv_obj_add_style(prev_btn, &style_btn, 0);
    lv_obj_add_event_cb(prev_btn, control_btn_event_cb, LV_EVENT_CLICKED, (void*)"PREV");
    lv_obj_t *prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, LV_SYMBOL_LEFT);
    lv_obj_center(prev_label);
    
    // Play/Pause button
    lv_obj_t *play_btn = lv_btn_create(controls_cont);
    lv_obj_set_size(play_btn, 50, 40);
    lv_obj_add_style(play_btn, &style_btn, 0);
    lv_obj_add_event_cb(play_btn, control_btn_event_cb, LV_EVENT_CLICKED, (void*)"PLAY");
    lv_obj_t *play_label = lv_label_create(play_btn);
    lv_label_set_text(play_label, LV_SYMBOL_PLAY);
    lv_obj_center(play_label);
    
    // Next button
    lv_obj_t *next_btn = lv_btn_create(controls_cont);
    lv_obj_set_size(next_btn, 50, 40);
    lv_obj_add_style(next_btn, &style_btn, 0);
    lv_obj_add_event_cb(next_btn, control_btn_event_cb, LV_EVENT_CLICKED, (void*)"NEXT");
    lv_obj_t *next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, LV_SYMBOL_RIGHT);
    lv_obj_center(next_label);
    
    // Shuffle button
    shuffle_btn = lv_btn_create(controls_cont);
    lv_obj_set_size(shuffle_btn, 50, 40);
    lv_obj_add_style(shuffle_btn, &style_btn, 0);
    lv_obj_add_event_cb(shuffle_btn, control_btn_event_cb, LV_EVENT_CLICKED, (void*)"SHUFFLE");
    lv_obj_t *shuffle_label = lv_label_create(shuffle_btn);
    lv_label_set_text(shuffle_label, LV_SYMBOL_REFRESH);
    lv_obj_center(shuffle_label);
    
    // Like button
    like_btn = lv_btn_create(controls_cont);
    lv_obj_set_size(like_btn, 50, 40);
    lv_obj_add_style(like_btn, &style_btn, 0);
    lv_obj_add_event_cb(like_btn, control_btn_event_cb, LV_EVENT_CLICKED, (void*)"LIKE");
    lv_obj_t *like_label = lv_label_create(like_btn);
    lv_label_set_text(like_label, LV_SYMBOL_OK);
    lv_obj_center(like_label);
}

void setup() {
    Serial.begin(115200);
    
    pinMode(BOARD_POWERON, OUTPUT);
    digitalWrite(BOARD_POWERON, HIGH);
    
    pinMode(BOARD_SDCARD_CS, OUTPUT);
    pinMode(RADIO_CS_PIN, OUTPUT);
    pinMode(BOARD_TFT_CS, OUTPUT);
    digitalWrite(BOARD_SDCARD_CS, HIGH);
    digitalWrite(RADIO_CS_PIN, HIGH);
    digitalWrite(BOARD_TFT_CS, HIGH);
    
    pinMode(BOARD_SPI_MISO, INPUT_PULLUP);
    SPI.begin(BOARD_SPI_SCK, BOARD_SPI_MISO, BOARD_SPI_MOSI);
    
    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
    
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    
    pinMode(BOARD_TOUCH_INT, INPUT);
    touch.setPins(-1, BOARD_TOUCH_INT);
    if (!touch.begin(Wire, GT911_SLAVE_ADDRESS_L)) {
        Serial.println("Touch initialization failed!");
        while (1) delay(1000);
    }
    touch.setMaxCoordinates(320, 240);
    touch.setSwapXY(true);
    touch.setMirrorXY(false, true);
    
    lv_init();
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t *buf = (lv_color_t *)ps_malloc(TFT_WIDTH * TFT_HEIGHT * sizeof(lv_color_t));
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, TFT_WIDTH * TFT_HEIGHT);
    
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TFT_HEIGHT;
    disp_drv.ver_res = TFT_WIDTH;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);

    init_styles();
    create_spotify_gui();
    
    pinMode(BOARD_BL_PIN, OUTPUT);
    analogWrite(BOARD_BL_PIN, 128);
}

void loop() {
    lv_timer_handler();
    
    Wire.requestFrom(LILYGO_KB_SLAVE_ADDRESS, 1);
    if (Wire.available() > 0) {
        char keyValue = Wire.read();
        if (keyValue != 0) {
            Serial.print("KEY:");
            Serial.println(keyValue);
        }
    }
    
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        if (cmd.startsWith("TRACK:")) {
            lv_label_set_text(now_playing_label, cmd.substring(6).c_str());
        } else if (cmd.startsWith("ARTIST:")) {
            lv_label_set_text(artist_label, cmd.substring(7).c_str());
        } else if (cmd.startsWith("PROGRESS:")) {
            lv_slider_set_value(progress_slider, cmd.substring(9).toInt(), LV_ANIM_ON);
        } else if (cmd.startsWith("TIME:")) {
            lv_label_set_text(progress_label, cmd.substring(5).c_str());
        }
    }
    
    delay(5);
}

static void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, false);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

static void touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    static int16_t x[5], y[5];
    data->state = LV_INDEV_STATE_REL;
    
    if (touch.isPressed()) {
        uint8_t touched = touch.getPoint(x, y, touch.getSupportTouchPoint());
        if (touched > 0) {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = x[0];
            data->point.y = y[0];
        }
    }
}

static void slider_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int value = lv_slider_get_value(slider);
    Serial.print("SEEK:");
    Serial.println(value);
}

static void control_btn_event_cb(lv_event_t *e) {
    const char *cmd = (const char *)lv_event_get_user_data(e);
    Serial.print("CMD:");
    Serial.println(cmd);
}