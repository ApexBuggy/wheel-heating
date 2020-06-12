This project contains the code and documentation for the wheel heating
project. This project was started by the team participating in
(Build18 2020)[http://www.build18.org/garage/project/577/], and was
continued by that group and other members of Apex.

## To-dos

* This repository still needs a lot of documentation, particularly for
  the electrical and mechanical assemblies
* The hardware for the project is still a prototype and needs a final revision
* The code has the core functionalities, but lacks several nice
  features and needs robust error checking
* The parts list below is very incomplete

## Parts

* [MLX90614 IR temperature sensor](https://www.adafruit.com/product/1748)
* [RobotDyn AC light dimmer module](https://robotdyn.com/ac-light-dimmer-module-1-channel-3-3v-5v-logic-ac-50-60hz-220v-110v.html)
* [16x2 LCD display with PCF8574 I2C adapter](https://www.amazon.com/JANSANE-Arduino-Display-Interface-Raspberry/dp/B07D83DY17/)

## Schematic

![Schematic](/schematic.svg)

This schematic was created using Circuit Diagram and is available
[here](https://crcit.net/c/bdd47728927a4dc786cfb47f31c64128). If the
schematic changes, it is possible to make a copy of this one and
modify the copy. Update the link in this README to be current.

## Code Dependencies

* [SparkFun MLX90614 Arduino Library](https://github.com/sparkfun/SparkFun_MLX90614_Arduino_Library).
  That library's README has instructions for installation.
* [RobotDyn Dimmer Library](https://github.com/RobotDynOfficial/RBDDimmer)
* [LiquidCrystal I2C PCF8574 Library](https://github.com/mathertel/LiquidCrystal_PCF8574)

## Initial Configuration

The tempurature sensors communicate over "an SMBus-compatible digital
interface", which is basically a subset of I2C. Each sensor comes
configured to use the 7-bit address 0x5A. We can't have multiple
devices with the same address on the same bus, but fortunately, we can
change each sensor's address. The SparkFun library linked above has an
example sketch to do exactly this, which can be found through the
Arduino IDE once the library is installed (File > Examples > SparkFun
MLX90614 > MLX90614\_Set\_Address), or
[here](https://github.com/sparkfun/SparkFun_MLX90614_Arduino_Library/blob/master/examples/MLX90614_Set_Address/MLX90614_Set_Address.ino).
For each new sensor, connect it to an arduino and give it a new
address by changing newAddress in the example. I used 0x5A, 0x5B, and
0x5C. They should all respond to address 0x0, so you should be able to
use that as the oldAddress if you aren't sure what the actual address
is.

## Additional Resources
* Adafruit has a good tutorial on using the IR thermometer:
  https://learn.adafruit.com/using-melexis-mlx90614-non-contact-sensors/wiring-and-test
