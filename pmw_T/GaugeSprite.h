#ifndef GAUGESPRITE_H
#define GAUGESPRITE_H

#include <TFT_eSPI.h>

class GaugeSprite {
private:
    TFT_eSprite* sprite;
    TFT_eSPI* tft;
    int16_t centerX;
    int16_t centerY;
    int16_t radius;
    uint16_t backgroundColor;
    uint16_t arcColor;
    uint16_t needleColor;
    float minValue;
    float maxValue;
    float currentValue;
    const char* label;
    const char* units;
    
    // Pre-calculated values for performance
    static const int ANGLE_START = 45;    // Start angle (45 degrees)
    static const int ANGLE_END = 315;     // End angle (315 degrees)
    static const int ANGLE_RANGE = 270;   // Total angle range (270 degrees)
    
    void drawArc(int16_t x, int16_t y, int16_t r, int16_t startAngle, int16_t endAngle, uint16_t color) {
        // Draw arc using integer math for better performance
        float sx = cos((startAngle - 90) * DEG_TO_RAD);
        float sy = sin((startAngle - 90) * DEG_TO_RAD);
        uint16_t x1 = sx * r + x;
        uint16_t y1 = sy * r + y;

        for (int i = startAngle; i < endAngle; i++) {
            float sx2 = cos((i + 1 - 90) * DEG_TO_RAD);
            float sy2 = sin((i + 1 - 90) * DEG_TO_RAD);
            int x2 = sx2 * r + x;
            int y2 = sy2 * r + y;
            sprite->drawLine(x1, y1, x2, y2, color);
            x1 = x2;
            y1 = y2;
        }
    }

public:
    GaugeSprite(TFT_eSPI* _tft, int16_t x, int16_t y, int16_t r, 
                const char* _label, const char* _units,
                float _min, float _max, uint16_t bgColor = TFT_BLACK) 
        : tft(_tft), centerX(x), centerY(y), radius(r),
          label(_label), units(_units),
          minValue(_min), maxValue(_max),
          backgroundColor(bgColor),
          arcColor(TFT_BLUE),
          needleColor(TFT_RED),
          currentValue(_min)
    {
        sprite = new TFT_eSprite(tft);
        sprite->createSprite(r * 2, r * 2);
        sprite->setTextDatum(MC_DATUM);
    }
    
    ~GaugeSprite() {
        if (sprite) {
            sprite->deleteSprite();
            delete sprite;
        }
    }

    void setValue(float value) {
        currentValue = constrain(value, minValue, maxValue);
    }

    void setColors(uint16_t arc, uint16_t needle) {
        arcColor = arc;
        needleColor = needle;
    }

    void draw() {
        sprite->fillSprite(backgroundColor);
        
        // Draw outer arc
        drawArc(radius, radius, radius - 2, ANGLE_START, ANGLE_END, arcColor);
        
        // Calculate needle angle
        float valueRange = maxValue - minValue;
        float valuePercent = (currentValue - minValue) / valueRange;
        int needleAngle = ANGLE_START + (valuePercent * ANGLE_RANGE);
        
        // Draw needle
        float nx = cos((needleAngle - 90) * DEG_TO_RAD);
        float ny = sin((needleAngle - 90) * DEG_TO_RAD);
        sprite->drawLine(radius, radius, 
                        radius + nx * (radius - 10),
                        radius + ny * (radius - 10),
                        needleColor);
        
        // Draw center hub
        sprite->fillCircle(radius, radius, 5, needleColor);
        
        // Draw value text
        sprite->setTextColor(TFT_WHITE);
        sprite->drawString(String(currentValue, 1) + units, radius, radius + 20);
        sprite->drawString(label, radius, radius - 20);
        
        // Push to screen
        sprite->pushSprite(centerX - radius, centerY - radius);
    }
};

#endif
