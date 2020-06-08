#include "dht.h"
#include <LiquidCrystal.h>
#include <Chrono.h>
#include <Fuzzy.h>

byte temp_icon[8] = {
       0b00100,
       0b00110,
       0b00100,
       0b00110,
       0b00100,
       0b01110,
       0b01110,
       0b00000
};

byte humidity_icon[8] = {
    0b00100,
    0b00100,
    0b01110,
    0b01110,
    0b10111,
    0b10111,
    0b01110,
    0b00000
};

#define DHT22PIN 7
#define FanGate 6

// Instantiating an objects

dht DHT22;
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
Chrono weatherChrono;
Chrono ledChrono;
Fuzzy* fuzzy = new Fuzzy();

const int redpin = 10; // red LED
const int bluepin = 9; // blue LED
const int greenpin = 8; // green LED

void setup()
{
    Serial.begin(115200);
    initializeLCD();
    initializeLED();
    initializeFAN();

    //randomSeed(analogRead(0));
    FuzzySet* cold = new FuzzySet(-30, -30, 5, 13);
    FuzzySet* warm = new FuzzySet(12, 18, 18, 28);
    FuzzySet* hot = new FuzzySet(25, 30, 30, 30);
    FuzzySet* too_hot = new FuzzySet(28, 32, 35, 35);

    FuzzySet* humidity_critical_low = new FuzzySet(0, 0, 15, 27);
    FuzzySet* humidity_low = new FuzzySet(25, 32.5, 32.5, 40);
    FuzzySet* humidity_good = new FuzzySet(38, 40, 60, 62);
    FuzzySet* humidity_high = new FuzzySet(60, 80, 100, 100);

    //output weather temperature
    FuzzySet* weather_too_cold = new FuzzySet(0, 20, 20, 40);
    FuzzySet* weather_average = new FuzzySet(30, 50, 50, 70);
    FuzzySet* weather_too_hot = new FuzzySet(60, 80, 80, 100);
   
    //output weather overall
    FuzzySet* very_bad = new FuzzySet(0, 12.5, 12.5, 25);
    FuzzySet* bad = new FuzzySet(20, 32.5, 32.5, 45);
    FuzzySet* normal = new FuzzySet(40, 52.5, 52.5, 65);
    FuzzySet* good = new FuzzySet(60, 72.5, 72.5, 85);
    FuzzySet* very_good = new FuzzySet(80, 90, 90, 100);



    //Fuzzy input temp
    FuzzyInput* temperature = new FuzzyInput(1);
    temperature->addFuzzySet(cold);
    temperature->addFuzzySet(warm);
    temperature->addFuzzySet(hot);
    temperature->addFuzzySet(too_hot);
    fuzzy->addFuzzyInput(temperature);
    // FuzzyInput humi
    FuzzyInput* humidity = new FuzzyInput(2);
    humidity->addFuzzySet(humidity_critical_low);
    humidity->addFuzzySet(humidity_low);
    humidity->addFuzzySet(humidity_good);
    humidity->addFuzzySet(humidity_high);
    fuzzy->addFuzzyInput(humidity);


    // FuzzyOutput
    FuzzyOutput* weather_temperature = new FuzzyOutput(1);
    weather_temperature->addFuzzySet(weather_too_cold);
    weather_temperature->addFuzzySet(weather_average);
    weather_temperature->addFuzzySet(weather_too_hot);
    fuzzy->addFuzzyOutput(weather_temperature);

    FuzzyOutput* weather_overall = new FuzzyOutput(2);
    weather_overall->addFuzzySet(very_bad);
    weather_overall->addFuzzySet(bad);
    weather_overall->addFuzzySet(normal);
    weather_overall->addFuzzySet(good);
    weather_overall->addFuzzySet(very_good);
    fuzzy->addFuzzyOutput(weather_overall);



    // rules for ventilator
    // if weather = cold -> too cold
    FuzzyRuleAntecedent* ifweathercold = new FuzzyRuleAntecedent();
    ifweathercold->joinSingle(cold);
    FuzzyRuleConsequent* thenweatherbad = new FuzzyRuleConsequent();
    thenweatherbad->addOutput(weather_too_cold);
    FuzzyRule* fuzzyRule01 = new FuzzyRule(1, ifweathercold, thenweatherbad);
    fuzzy->addFuzzyRule(fuzzyRule01);
    //  if weather = warm -> weather average
    FuzzyRuleAntecedent* ifweathermid = new FuzzyRuleAntecedent();
    ifweathermid->joinSingle(warm);
    FuzzyRuleConsequent* thenweatheraverage = new FuzzyRuleConsequent();
    thenweatheraverage->addOutput(weather_average);
    FuzzyRule* fuzzyRule02 = new FuzzyRule(2, ifweathermid, thenweatheraverage);
    fuzzy->addFuzzyRule(fuzzyRule02);
    // if weather = hot -> too hot
    FuzzyRuleAntecedent* ifweatherhot = new FuzzyRuleAntecedent();
    ifweatherhot->joinWithOR(hot, too_hot);
    FuzzyRuleConsequent* thenweather_too_hot = new FuzzyRuleConsequent();
    thenweather_too_hot->addOutput(weather_too_hot);
    FuzzyRule* fuzzyRule03 = new FuzzyRule(3, ifweatherhot, thenweather_too_hot);
    fuzzy->addFuzzyRule(fuzzyRule03);



    // za sucho, zostan w domu - red light
    FuzzyRuleAntecedent* deadly_humidity = new FuzzyRuleAntecedent();
    deadly_humidity->joinWithOR(humidity_critical_low, too_hot);
    FuzzyRuleConsequent* dont_go_outside = new FuzzyRuleConsequent();
    dont_go_outside->addOutput(very_bad);
    FuzzyRule* fuzzyRule4 = new FuzzyRule(4, deadly_humidity, dont_go_outside);
    fuzzy->addFuzzyRule(fuzzyRule4);


    //pretty important rule, will be used couple of times
    FuzzyRuleAntecedent* check_bad_humidity = new FuzzyRuleAntecedent();
    check_bad_humidity->joinWithOR(humidity_high, humidity_low);
    //
    FuzzyRuleAntecedent* if_weather_not_good = new FuzzyRuleAntecedent();
    if_weather_not_good->joinWithAND(check_bad_humidity, cold);
    FuzzyRuleConsequent* not_good = new FuzzyRuleConsequent();
    not_good->addOutput(bad);
    FuzzyRule* fuzzyRule5 = new FuzzyRule(5, if_weather_not_good, not_good);
    fuzzy->addFuzzyRule(fuzzyRule5);

    FuzzyRuleAntecedent* if_weather_warm_and_bad_humidity = new FuzzyRuleAntecedent();
    if_weather_warm_and_bad_humidity->joinWithAND(check_bad_humidity, warm);
    FuzzyRuleAntecedent* if_weather_cold_and_good_humidity = new FuzzyRuleAntecedent();
    if_weather_cold_and_good_humidity->joinWithAND(humidity_good, cold);
    FuzzyRuleAntecedent* if_weather_warm_and_bad_humidity_or_if_weather_cold_and_good_humidity = new FuzzyRuleAntecedent();
    if_weather_warm_and_bad_humidity_or_if_weather_cold_and_good_humidity->joinWithOR(if_weather_warm_and_bad_humidity, if_weather_cold_and_good_humidity);
    FuzzyRuleConsequent* okay = new FuzzyRuleConsequent();
    okay->addOutput(normal);
    FuzzyRule* fuzzyRule6 = new FuzzyRule(6, if_weather_warm_and_bad_humidity_or_if_weather_cold_and_good_humidity, okay);
    fuzzy->addFuzzyRule(fuzzyRule6);

    FuzzyRuleAntecedent* if_weather_hot_and_bad_humidity = new FuzzyRuleAntecedent();
    if_weather_hot_and_bad_humidity->joinWithAND(check_bad_humidity, hot);
    FuzzyRuleAntecedent* if_weather_warm_and_good_humidity = new FuzzyRuleAntecedent();
    if_weather_warm_and_good_humidity->joinWithAND(humidity_good, cold);
    FuzzyRuleAntecedent* if_weather_hot_and_bad_humidity_or_if_weather_warm_and_good_humidity = new FuzzyRuleAntecedent();
    if_weather_hot_and_bad_humidity_or_if_weather_warm_and_good_humidity->joinWithOR(if_weather_hot_and_bad_humidity, if_weather_warm_and_good_humidity);
    FuzzyRuleConsequent* good_already = new FuzzyRuleConsequent();
    good_already->addOutput(good);
    FuzzyRule* fuzzyRule7 = new FuzzyRule(7, if_weather_hot_and_bad_humidity_or_if_weather_warm_and_good_humidity, good_already);
    fuzzy->addFuzzyRule(fuzzyRule7);

    FuzzyRuleAntecedent* if_weather_hot_and_humidity_good = new FuzzyRuleAntecedent();
    if_weather_hot_and_humidity_good->joinWithAND(hot, humidity_good);
    FuzzyRuleConsequent* best = new FuzzyRuleConsequent();
    best->addOutput(very_good);
    FuzzyRule* fuzzyRule8 = new FuzzyRule(8, if_weather_hot_and_humidity_good, best);
    fuzzy->addFuzzyRule(fuzzyRule8);

}

void loop()
{
    if (weatherChrono.hasPassed(1000)){
        lcd.clear();
        float temp, humidity ;
        
        weather_sensor(temp, humidity);

        //int input = random(0, 80);
        Serial.println(temp);
        Serial.println(humidity);
        fuzzy->setInput(1, (int)temp);
        fuzzy->setInput(2, (int)humidity);
        fuzzy->fuzzify();
        float steruj_wiatrakiem = fuzzy->defuzzify(1);
        float steruj_dioda = fuzzy->defuzzify(2);
        Serial.println(steruj_wiatrakiem);
        Serial.println(steruj_dioda);
        //delay(12000);


        weatherChrono.restart();
    }

    if (Serial.available()) {
        char ch = Serial.read();
        if (ch >= '0' && ch <= '9') {
            int speed = ch - '0';
            analogWrite(FanGate, map(speed, 0, 9, 0, 255));
        }
    }

    //diode();

}

void initializeLCD()
{
    lcd.begin(16, 2);
    lcd.createChar(1, temp_icon);
    lcd.createChar(2, humidity_icon);
}

void initializeLED()
{
    pinMode(redpin, OUTPUT);
    pinMode(bluepin, OUTPUT);
    pinMode(greenpin, OUTPUT);
}

void initializeFAN()
{
    pinMode(FanGate, OUTPUT);
    digitalWrite(FanGate, LOW); //temp
}

void weather_sensor(float &temp, float &humidity)
{
    int chk = DHT22.read(DHT22PIN);

    switch (chk)
    {
    case DHTLIB_OK:
        Serial.print("OKt");
        break;
    case DHTLIB_ERROR_CHECKSUM:
        Serial.println("B³¹d sumy kontrolnej");
        break;
    case DHTLIB_ERROR_TIMEOUT:
        Serial.println("Koniec czasu oczekiwania - brak odpowiedzi");
        break;
    default:
        Serial.println("Nieznany b³¹d");
        break;
    }

    temp = DHT22.temperature;
    humidity = DHT22.humidity;
    
    lcd.write(1);
    lcd.setCursor(2, 0);
    lcd.print(temp, 0);
    lcd.print((char)223); //degree sign
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.write(2);
    lcd.setCursor(2, 1);
    lcd.print(humidity, 0);
    lcd.print("%");


    Serial.print("Wilgotnosc (%): ");
    Serial.print(humidity, 2);
    Serial.print("tt");
    Serial.print("Temperatura (C): ");
    Serial.println(temp, 2);
}

void diode() 
{
    if (ledChrono.hasPassed(1)) {
        ledChrono.restart();
        for (int val = 255; val > 0; val--)
        {
            analogWrite(redpin, val);  //set PWM value for red
            analogWrite(bluepin, 255 - val); //set PWM value for blue
            analogWrite(greenpin, 128 - val); //set PWM value for green
        }
    }
    if (ledChrono.hasPassed(1)) {
        ledChrono.restart();
        for (int val = 0; val < 255; val++)
        {
            analogWrite(redpin, val);  //set PWM value for red
            analogWrite(bluepin, 255 - val); //set PWM value for blue
            analogWrite(greenpin, 128 - val); //set PWM value for green
        }
    }
}

//void initializeBMP()
//{
//    if (!bmp280.initialize()) {
//        Serial.println("BMP missing");
//    }
//
//    bmp280.setEnabled(0);
//    bmp280.triggerMeasurement();
//}