// Function to draw a gauge
void drawGauge(float value, float maxValue, int x, int y, String label) {
    int radius = 40;
    float angle = map(value, 0, maxValue, 0, 180); // Map value to angle
    tft.drawArc(x, y, radius, 0, angle, TFT_GREEN); // Draw gauge arc
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(x - 20, y + 50);
    tft.print(label + ": " + String(value, 1));
}

// Update displayTemp to include gauges
void displayTemp() {
    // Existing temperature display code...

    // Draw gauges for temperature and humidity
    drawGauge(temp, 100, 60, 250, "Temp"); // Example: 100 as max temp
    drawGauge(humidity, 100, 180, 250, "Humidity"); // Example: 100 as max humidity

    // Draw back button