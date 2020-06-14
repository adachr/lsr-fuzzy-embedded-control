#include "dht.h"
#include <LiquidCrystal.h>
#include <Chrono.h>
#include <OneWire.h>
#include <DallasTemperature.h>
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

typedef struct rgb_struct {
    int red;
    int green;
    int blue;
} RGB;

#define DHT22PIN 7
#define FanGate 6
#define ONE_WIRE_BUS 13  // on pin 13 (a 4.7K resistor is necessary)

// Instantiating an objects
dht DHT22;
DeviceAddress thermometerAddress;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);

LiquidCrystal lcd(12, 8, 5, 4, 3, 2);
Chrono ledChrono;
Chrono weatherChrono;

const int redpin = 11;
const int greenpin = 10;
const int bluepin = 9;

RGB red = { 255,0,0 };
RGB yellow = { 255,255,0 };
RGB magenta = { 255,0,255 };
RGB green = { 0,255,0 };

Fuzzy* fuzzy = new Fuzzy();

// FuzzyInput fan
FuzzySet *_very_cold = new FuzzySet(-55, -55, 0, 20);
FuzzySet *_cold =      new FuzzySet(20, 30, 30, 40);
FuzzySet *_warm =      new FuzzySet(30, 40, 40, 50);
FuzzySet *_hot =       new FuzzySet(40, 50, 50, 60);
FuzzySet *_very_hot =  new FuzzySet(50, 60, 125, 125);

// FuzzyOutput fan
FuzzySet *_zero =      new FuzzySet(0, 0, 0, 0);
FuzzySet *_slow =      new FuzzySet(45, 85, 85, 130);
FuzzySet *_medium =    new FuzzySet(85, 130, 130, 170);
FuzzySet *_fast =      new FuzzySet(130, 170, 170, 210);
FuzzySet *_very_fast = new FuzzySet(210, 255, 255, 255);

//-------------------------------------------------------------------------

// FuzzyInput temp
//FuzzySet *low =     new FuzzySet(19.5, 19.5, 19.5, 22.5);
//FuzzySet *optimal = new FuzzySet(19.5, 22.5, 24.5, 27);
//FuzzySet *high =    new FuzzySet(24.5, 27, 27, 27);

FuzzySet *really_cold = new FuzzySet(-30, -30, 5, 7);
FuzzySet *cold = new FuzzySet(6, 15, 15, 18);
FuzzySet *warm = new FuzzySet(17, 22, 24, 27);
FuzzySet *hot = new FuzzySet(25, 29, 29, 33);
FuzzySet *too_hot = new FuzzySet(31, 34, 35, 35);

// FuzzyInput humidity
FuzzySet *humidity_critical_low = new FuzzySet(0, 0, 15, 25);
FuzzySet *humidity_low =          new FuzzySet(20, 28, 28, 32);
FuzzySet *humidity_good =         new FuzzySet(30, 40, 55, 62);
FuzzySet *humidity_high =         new FuzzySet(60, 80, 100, 100);

// FuzzyOutput weather overall
FuzzySet *very_bad =  new FuzzySet(0, 12.5, 12.5, 25);
FuzzySet *bad =       new FuzzySet(20, 32.5, 32.5, 45);
FuzzySet *normal =    new FuzzySet(40, 52.5, 52.5, 65);
FuzzySet *good =      new FuzzySet(60, 72.5, 72.5, 85);
FuzzySet *very_good = new FuzzySet(80, 90, 90, 100);

//-------------------------------------------------------------------------

static float toCelsius(float fromFahrenheit) { return (fromFahrenheit - 32.0) / 1.8; };
static float toFahrenheit(float fromCelcius) { return 1.8 * fromCelcius + 32.0; };

//boolean isFahrenheit: True == Fahrenheit; False == Celcius
float computeDewPoint(float temperature, float percentHumidity, bool isFahrenheit = false)
{
    // reference: http://wahiduddin.net/calc/density_algorithms.htm
    if (isFahrenheit) {
        temperature = toCelsius(temperature);
    }
    double A0 = 373.15 / (273.15 + (double)temperature);
    double SUM = -7.90298 * (A0 - 1);
    SUM += 5.02808 * log10(A0);
    SUM += -1.3816e-7 * (pow(10, (11.344 * (1 - 1 / A0))) - 1);
    SUM += 8.1328e-3 * (pow(10, (-3.49149 * (A0 - 1))) - 1);
    SUM += log10(1013.246);
    double VP = pow(10, SUM - 3) * (double)percentHumidity;
    double Td = log(VP / 0.61078); // temp var
    Td = (241.88 * Td) / (17.558 - Td);
    return isFahrenheit ? toFahrenheit(Td) : Td;
}


void setup()
{
    Serial.begin(115200);
    initializeLCD();
    initializeLED();
    initializeFAN();
    initializeTempProbe();

    //FuzzyInputSet1 for fan control
    FuzzyInput *temperature = new FuzzyInput(1);
    temperature->addFuzzySet(_very_cold);
    temperature->addFuzzySet(_cold);
    temperature->addFuzzySet(_warm);
    temperature->addFuzzySet(_hot);
    temperature->addFuzzySet(_very_hot);
    fuzzy->addFuzzyInput(temperature);

    //FuzzyOutputSet1 for fan control
    FuzzyOutput *speed = new FuzzyOutput(1);
    speed->addFuzzySet(_zero);
    speed->addFuzzySet(_slow);
    speed->addFuzzySet(_medium);
    speed->addFuzzySet(_fast);
    speed->addFuzzySet(_very_fast);
    fuzzy->addFuzzyOutput(speed);

    // Building FuzzyRule 1
    FuzzyRuleAntecedent *ifTempVeryCold = new FuzzyRuleAntecedent();
    ifTempVeryCold->joinSingle(_very_cold);
    FuzzyRuleConsequent *thenZero = new FuzzyRuleConsequent();
    thenZero->addOutput(_zero);
    FuzzyRule *fanFuzzyRule1 = new FuzzyRule(1, ifTempVeryCold, thenZero);
    fuzzy->addFuzzyRule(fanFuzzyRule1);

    // Building FuzzyRule 2
    FuzzyRuleAntecedent *ifTempCold = new FuzzyRuleAntecedent();
    ifTempCold->joinSingle(_cold);
    FuzzyRuleConsequent *thenSlow = new FuzzyRuleConsequent();
    thenSlow->addOutput(_slow);
    FuzzyRule *fanFuzzyRule2 = new FuzzyRule(2, ifTempCold, thenSlow);
    fuzzy->addFuzzyRule(fanFuzzyRule2);

    // Building FuzzyRule 3
    FuzzyRuleAntecedent *ifTempWarm = new FuzzyRuleAntecedent();
    ifTempWarm->joinSingle(_warm);
    FuzzyRuleConsequent *thenMedium = new FuzzyRuleConsequent();
    thenMedium->addOutput(_medium);
    FuzzyRule *fanFuzzyRule3 = new FuzzyRule(3, ifTempWarm, thenMedium);
    fuzzy->addFuzzyRule(fanFuzzyRule3);
    
    // Building FuzzyRule 4
    FuzzyRuleAntecedent *ifTempHot = new FuzzyRuleAntecedent();
    ifTempHot->joinSingle(_hot);
    FuzzyRuleConsequent *thenFast = new FuzzyRuleConsequent();
    thenFast->addOutput(_fast);
    FuzzyRule *fanFuzzyRule4 = new FuzzyRule(4, ifTempHot, thenFast);
    fuzzy->addFuzzyRule(fanFuzzyRule4);

    // Building FuzzyRule 5
    FuzzyRuleAntecedent *ifTempVeryHot = new FuzzyRuleAntecedent();
    ifTempVeryHot->joinSingle(_very_hot);
    FuzzyRuleConsequent *thenVeyFast = new FuzzyRuleConsequent();
    thenVeyFast->addOutput(_very_fast);
    FuzzyRule *fanFuzzyRule5 = new FuzzyRule(5, ifTempVeryHot, thenVeyFast);
    fuzzy->addFuzzyRule(fanFuzzyRule5);

    //----------------------------------------------------------------------------

    ////randomSeed(analogRead(0));
    //FuzzySet* cold = new FuzzySet(-30, -30, 5, 13);
    //FuzzySet* warm = new FuzzySet(12, 18, 18, 28);
    //FuzzySet* hot = new FuzzySet(25, 30, 30, 30);
    //FuzzySet* too_hot = new FuzzySet(28, 32, 35, 35);

    //FuzzySet* humidity_critical_low = new FuzzySet(0, 0, 15, 27);
    //FuzzySet* humidity_low = new FuzzySet(25, 32.5, 32.5, 40);
    //FuzzySet* humidity_good = new FuzzySet(38, 40, 60, 62);
    //FuzzySet* humidity_high = new FuzzySet(60, 80, 100, 100);

    ////output weather temperature
    //FuzzySet* weather_too_cold = new FuzzySet(0, 20, 20, 40);
    //FuzzySet* weather_average = new FuzzySet(30, 50, 50, 70);
    //FuzzySet* weather_too_hot = new FuzzySet(60, 80, 80, 100);
   
    ////output weather overall
    //FuzzySet* very_bad = new FuzzySet(0, 12.5, 12.5, 25);
    //FuzzySet* bad = new FuzzySet(20, 32.5, 32.5, 45);
    //FuzzySet* normal = new FuzzySet(40, 52.5, 52.5, 65);
    //FuzzySet* good = new FuzzySet(60, 72.5, 72.5, 85);
    //FuzzySet* very_good = new FuzzySet(80, 90, 90, 100);



    //FuzzyInputSet2 weather_temperature
    FuzzyInput *weather_temperature = new FuzzyInput(2);
    weather_temperature->addFuzzySet(really_cold);
    weather_temperature->addFuzzySet(cold);
    weather_temperature->addFuzzySet(warm);
    weather_temperature->addFuzzySet(hot);
    weather_temperature->addFuzzySet(too_hot);
    fuzzy->addFuzzyInput(weather_temperature);
    
    // FuzzyInputSet3 humidity
    FuzzyInput *humidity = new FuzzyInput(3);
    humidity->addFuzzySet(humidity_critical_low);
    humidity->addFuzzySet(humidity_low);
    humidity->addFuzzySet(humidity_good);
    humidity->addFuzzySet(humidity_high);
    fuzzy->addFuzzyInput(humidity);


    //// FuzzyOutput
    //FuzzyOutput* weather_temperature = new FuzzyOutput(1);
    //weather_temperature->addFuzzySet(weather_too_cold);
    //weather_temperature->addFuzzySet(weather_average);
    //weather_temperature->addFuzzySet(weather_too_hot);
    //fuzzy->addFuzzyOutput(weather_temperature);

    //FuzzyOutputSet2 for weather_overall
    FuzzyOutput *weather_overall = new FuzzyOutput(2);
    weather_overall->addFuzzySet(very_bad);
    weather_overall->addFuzzySet(bad);
    weather_overall->addFuzzySet(normal);
    weather_overall->addFuzzySet(good);
    weather_overall->addFuzzySet(very_good);
    fuzzy->addFuzzyOutput(weather_overall);

    // improved MM

    // very_bad
    FuzzyRuleAntecedent* deadly_humidity = new FuzzyRuleAntecedent();
    deadly_humidity->joinWithOR(humidity_critical_low, too_hot);
    FuzzyRuleAntecedent* deadly_humidity2 = new FuzzyRuleAntecedent();
    deadly_humidity2->joinWithOR(deadly_humidity, really_cold);
    FuzzyRuleConsequent* dont_go_outside = new FuzzyRuleConsequent();
    dont_go_outside->addOutput(very_bad);
    FuzzyRule* fuzzyRule1 = new FuzzyRule(1, deadly_humidity2, dont_go_outside);
    fuzzy->addFuzzyRule(fuzzyRule1);

    // bad
    FuzzyRuleAntecedent* if_hot_and_high = new FuzzyRuleAntecedent();
    if_hot_and_high->joinWithAND(hot, humidity_high);
    FuzzyRuleAntecedent* if_cold_and_low = new FuzzyRuleAntecedent();
    if_cold_and_low->joinWithAND(cold, humidity_low);
    FuzzyRuleAntecedent* thisORthat = new FuzzyRuleAntecedent();
    thisORthat->joinWithOR(if_hot_and_high, if_cold_and_low);
    FuzzyRuleConsequent* not_good = new FuzzyRuleConsequent();
    not_good->addOutput(bad);
    FuzzyRule* fuzzyRule2 = new FuzzyRule(2, thisORthat, not_good);
    fuzzy->addFuzzyRule(fuzzyRule2);

    // normal
    FuzzyRuleAntecedent* if_high_or_okay = new FuzzyRuleAntecedent();
    if_high_or_okay->joinWithOR(humidity_good, humidity_high);
    FuzzyRuleAntecedent* if_cold_and_high_or_okay = new FuzzyRuleAntecedent();
    if_cold_and_high_or_okay->joinWithAND(cold, if_high_or_okay);
    FuzzyRuleAntecedent* if_hot_and_low = new FuzzyRuleAntecedent();
    if_hot_and_low->joinWithAND(hot, humidity_low);
    FuzzyRuleAntecedent* thisORthat2 = new FuzzyRuleAntecedent();
    thisORthat2->joinWithOR(if_cold_and_high_or_okay, if_hot_and_low);
    FuzzyRuleConsequent* okay = new FuzzyRuleConsequent();
    okay->addOutput(normal);
    FuzzyRule* fuzzyRule3 = new FuzzyRule(3, thisORthat2, okay);
    fuzzy->addFuzzyRule(fuzzyRule3);

    //check bad humidty (low or high)
    FuzzyRuleAntecedent* check_bad_humidity = new FuzzyRuleAntecedent();
    check_bad_humidity->joinWithOR(humidity_high, humidity_low);
    FuzzyRuleAntecedent* if_weather_warm_and_bad_humidity = new FuzzyRuleAntecedent();
    if_weather_warm_and_bad_humidity->joinWithAND(check_bad_humidity, warm);
    FuzzyRuleAntecedent* if_weather_hot_and_good_humidity = new FuzzyRuleAntecedent();
    if_weather_hot_and_good_humidity->joinWithAND(humidity_good, hot);
    FuzzyRuleAntecedent* this_or_that3 = new FuzzyRuleAntecedent();
    this_or_that3->joinWithOR(if_weather_hot_and_good_humidity, if_weather_warm_and_bad_humidity);
    FuzzyRuleConsequent* good_already = new FuzzyRuleConsequent();
    good_already->addOutput(good);
    FuzzyRule* fuzzyRule4 = new FuzzyRule(4, this_or_that3, good_already);
    fuzzy->addFuzzyRule(fuzzyRule4);

    FuzzyRuleAntecedent* if_weather_warm_and_humidity_good = new FuzzyRuleAntecedent();
    if_weather_warm_and_humidity_good->joinWithAND(warm, humidity_good);
    FuzzyRuleConsequent* best = new FuzzyRuleConsequent();
    best->addOutput(very_good);
    FuzzyRule* fuzzyRule5 = new FuzzyRule(5, if_weather_warm_and_humidity_good, best);
    fuzzy->addFuzzyRule(fuzzyRule5);


    //// rules for ventilator
    //// if weather = cold -> too cold
    //FuzzyRuleAntecedent* ifweathercold = new FuzzyRuleAntecedent();
    //ifweathercold->joinSingle(cold);
    //FuzzyRuleConsequent* thenweatherbad = new FuzzyRuleConsequent();
    //thenweatherbad->addOutput(weather_too_cold);
    //FuzzyRule* fuzzyRule01 = new FuzzyRule(1, ifweathercold, thenweatherbad);
    //fuzzy->addFuzzyRule(fuzzyRule01);
    ////  if weather = warm -> weather average
    //FuzzyRuleAntecedent* ifweathermid = new FuzzyRuleAntecedent();
    //ifweathermid->joinSingle(warm);
    //FuzzyRuleConsequent* thenweatheraverage = new FuzzyRuleConsequent();
    //thenweatheraverage->addOutput(weather_average);
    //FuzzyRule* fuzzyRule02 = new FuzzyRule(2, ifweathermid, thenweatheraverage);
    //fuzzy->addFuzzyRule(fuzzyRule02);
    
    //// if weather = hot -> too hot
    //FuzzyRuleAntecedent* ifweatherhot = new FuzzyRuleAntecedent();
    //ifweatherhot->joinWithOR(hot, too_hot);
    //FuzzyRuleConsequent* thenweather_too_hot = new FuzzyRuleConsequent();
    //thenweather_too_hot->addOutput(weather_too_hot);
    //FuzzyRule* fuzzyRule03 = new FuzzyRule(3, ifweatherhot, thenweather_too_hot);
    //fuzzy->addFuzzyRule(fuzzyRule03);



    //// za sucho, zostan w domu - red light
    //FuzzyRuleAntecedent* deadly_humidity = new FuzzyRuleAntecedent();
    //deadly_humidity->joinWithOR(humidity_critical_low, too_hot);
    //FuzzyRuleConsequent* dont_go_outside = new FuzzyRuleConsequent();
    //dont_go_outside->addOutput(very_bad);
    //FuzzyRule* fuzzyRule4 = new FuzzyRule(4, deadly_humidity, dont_go_outside);
    //fuzzy->addFuzzyRule(fuzzyRule4);


    ////pretty important rule, will be used couple of times
    //FuzzyRuleAntecedent* check_bad_humidity = new FuzzyRuleAntecedent();
    //check_bad_humidity->joinWithOR(humidity_high, humidity_low);
    ////
    //FuzzyRuleAntecedent* if_weather_not_good = new FuzzyRuleAntecedent();
    //if_weather_not_good->joinWithAND(check_bad_humidity, cold);
    //FuzzyRuleConsequent* not_good = new FuzzyRuleConsequent();
    //not_good->addOutput(bad);
    //FuzzyRule* fuzzyRule5 = new FuzzyRule(5, if_weather_not_good, not_good);
    //fuzzy->addFuzzyRule(fuzzyRule5);

    //FuzzyRuleAntecedent* if_weather_warm_and_bad_humidity = new FuzzyRuleAntecedent();
    //if_weather_warm_and_bad_humidity->joinWithAND(check_bad_humidity, warm);
    //FuzzyRuleAntecedent* if_weather_cold_and_good_humidity = new FuzzyRuleAntecedent();
    //if_weather_cold_and_good_humidity->joinWithAND(humidity_good, cold);
    //FuzzyRuleAntecedent* if_weather_warm_and_bad_humidity_or_if_weather_cold_and_good_humidity = new FuzzyRuleAntecedent();
    //if_weather_warm_and_bad_humidity_or_if_weather_cold_and_good_humidity->joinWithOR(if_weather_warm_and_bad_humidity, if_weather_cold_and_good_humidity);
    //FuzzyRuleConsequent* okay = new FuzzyRuleConsequent();
    //okay->addOutput(normal);
    //FuzzyRule* fuzzyRule6 = new FuzzyRule(6, if_weather_warm_and_bad_humidity_or_if_weather_cold_and_good_humidity, okay);
    //fuzzy->addFuzzyRule(fuzzyRule6);

    //FuzzyRuleAntecedent* if_weather_hot_and_bad_humidity = new FuzzyRuleAntecedent();
    //if_weather_hot_and_bad_humidity->joinWithAND(check_bad_humidity, hot);
    //FuzzyRuleAntecedent* if_weather_warm_and_good_humidity = new FuzzyRuleAntecedent();
    //if_weather_warm_and_good_humidity->joinWithAND(humidity_good, cold);
    //FuzzyRuleAntecedent* if_weather_hot_and_bad_humidity_or_if_weather_warm_and_good_humidity = new FuzzyRuleAntecedent();
    //if_weather_hot_and_bad_humidity_or_if_weather_warm_and_good_humidity->joinWithOR(if_weather_hot_and_bad_humidity, if_weather_warm_and_good_humidity);
    //FuzzyRuleConsequent* good_already = new FuzzyRuleConsequent();
    //good_already->addOutput(good);
    //FuzzyRule* fuzzyRule7 = new FuzzyRule(7, if_weather_hot_and_bad_humidity_or_if_weather_warm_and_good_humidity, good_already);
    //fuzzy->addFuzzyRule(fuzzyRule7);

    //FuzzyRuleAntecedent* if_weather_hot_and_humidity_good = new FuzzyRuleAntecedent();
    //if_weather_hot_and_humidity_good->joinWithAND(hot, humidity_good);
    //FuzzyRuleConsequent* best = new FuzzyRuleConsequent();
    //best->addOutput(very_good);
    //FuzzyRule* fuzzyRule8 = new FuzzyRule(8, if_weather_hot_and_humidity_good, best);
    //fuzzy->addFuzzyRule(fuzzyRule8);

}

void loop()
{
    if (weatherChrono.hasPassed(500)){
        float temp, humidity, dew_point;
        
        temp_sensor(temp);
        humidity_sensor(humidity);

        dew_point = computeDewPoint(temp, humidity);
        Serial.println(dew_point);
        fuzzy->setInput(1, temp);
        fuzzy->fuzzify();
        float steruj_wiatrakiem = fuzzy->defuzzify(1);

        fan(steruj_wiatrakiem);

        //int input = random(0, 80);
        /*fuzzy->setInput(1, (int)temp);
        fuzzy->setInput(2, (int)humidity);
        fuzzy->fuzzify();
        float steruj_wiatrakiem = fuzzy->defuzzify(1);
        float steruj_dioda = fuzzy->defuzzify(2);
        Serial.println(steruj_wiatrakiem);
        Serial.println(steruj_dioda);*/
        //delay(12000);

        weatherChrono.restart();
    }

    /*if (Serial.available()) {
        char ch = Serial.read();
        if (ch >= '0' && ch <= '9') {
            int speed = ch - '0';
            analogWrite(FanGate, map(speed, 0, 9, 0, 255));
        }
    }*/

    diode();


}
void initializeTempProbe()
{
    tempSensor.begin();

    if (!tempSensor.getAddress(thermometerAddress, 0))
        Serial.println("Unable to find Device.");
    else {
        Serial.print("Device 0 Address: ");
        printAddress(thermometerAddress);
        Serial.println();
    }

    tempSensor.setResolution(thermometerAddress, 12);
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

void humidity_sensor(float &humidity)
{
    DHT22.read(DHT22PIN);

    //temp = DHT22.temperature;
    humidity = DHT22.humidity;
    
    //Serial.print("Temperatura (C): ");
    //Serial.println(temp, 2);

    Serial.print("Wilgotnosc (%): ");
    Serial.println(humidity, 2);

    //lcd.write(1);
    //lcd.setCursor(2, 0);
    //lcd.print(temp, 0);
    //lcd.print((char)223); //degree sign
    //lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.write(2);
    lcd.setCursor(2, 1);
    lcd.print(humidity, 0);
    lcd.print("%");
}

void temp_sensor(float& temp)
{
    tempSensor.requestTemperatures();
    temp = tempSensor.getTempC(thermometerAddress);

    Serial.print("Temperatura (C): ");
    Serial.println(temp, 2);

    lcd.setCursor(0, 0);
    lcd.write(1);
    lcd.setCursor(2, 0);
    lcd.print(temp, 1);
    lcd.print((char)223); //degree sign
    lcd.print("C");
}

void fan(int speed)
{
    Serial.println(speed);
    analogWrite(FanGate, speed);
}

void diode() 
{
    writeRGB(&magenta);

    //if (ledChrono.hasPassed(1)) {
    //    ledChrono.restart();
    //    for (int val = 255; val > 0; val--)
    //    {
    //        analogWrite(redpin, val);  //set PWM value for red
    //        analogWrite(bluepin, 255 - val); //set PWM value for blue
    //        analogWrite(greenpin, 128 - val); //set PWM value for green
    //    }
    //}
    //if (ledChrono.hasPassed(1)) {
    //    ledChrono.restart();
    //    for (int val = 0; val < 255; val++)
    //    {
    //        analogWrite(redpin, val);  //set PWM value for red
    //        analogWrite(bluepin, 255 - val); //set PWM value for blue
    //        analogWrite(greenpin, 128 - val); //set PWM value for green
    //    }
    //}
}

// print device address from the address array
void printAddress(DeviceAddress deviceAddress)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (deviceAddress[i] < 16) Serial.print("0");
        Serial.print(deviceAddress[i], HEX);
    }
}

void writeRGB(struct rgb_struct* rgb) {
    analogWrite(redpin, rgb->red);
    analogWrite(greenpin, rgb->green);
    analogWrite(bluepin, rgb->blue);
}