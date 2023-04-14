#ifndef BLink_h
#define BLink_h

#include <Arduino.h>

#define BLINK_PIN 2

void blink(int times, int dl)
{
    digitalWrite(BLINK_PIN, LOW);
    bool led_on = false;
    for (int i = 0; i <= times * 2; i++)
    {
        delay(dl);
        led_on = !led_on;
        digitalWrite(BLINK_PIN, led_on ? LOW : HIGH);
    }
    digitalWrite(BLINK_PIN, HIGH);
}

void blink_on () {
    digitalWrite(BLINK_PIN, LOW);
}

void blink_off () {
    digitalWrite(BLINK_PIN, HIGH);
}

void blink(int times)
{
    blink(times, 100);
}

void blink()
{
    blink(2, 100);
}

void blink_err()
{
    blink(3, 150);
}

void blink_ok()
{
    blink(3, 300);
}

#endif