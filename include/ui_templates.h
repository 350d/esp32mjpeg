#pragma once

// UI template constants for ESP32-CAM web interface
// This file contains embedded HTML, CSS, and JavaScript templates

// CSS styles as string constant
extern const char* CSS_MAIN;

// JavaScript files as string constants  
extern const char* JS_CONTROLS;
extern const char* JS_FPS_COUNTER;
extern const char* JS_STATUS;

// HTML template as string constant
extern const char* HTML_TEMPLATE;

// Function to generate complete HTML page with substituted values
String generateHTMLPage(
    int quality, int brightness, int contrast, int saturation,
    int gainceiling, int colorbar, int awb, int agc, int aec,
    int hmirror, int vflip, int awb_gain, int agc_gain, int aec_value,
    int aec2, int dcw, int bpc, int wpc, int raw_gma, int lenc,
    int special_effect, int wb_mode, int ae_level
);
