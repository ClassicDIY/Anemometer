# Anemometer
Wireless WIFI Anemometer using ESP32

[![Hits](https://hits.seeyoufarm.com/api/count/incr/badge.svg?url=https%3A%2F%2Fgithub.com%2FClassicDIY%2FAnemometer&count_bg=%2379C83D&title_bg=%23555555&icon=&icon_color=%23E7E7E7&title=hits&edge_flat=false)](https://hits.seeyoufarm.com)

[![GitHub stars](https://img.shields.io/github/stars/ClassicDIY/Anemometer?style=for-the-badge)](https://github.com/ClassicDIY/Anemometer/stargazers)

<a href="https://www.buymeacoffee.com/r4K2HIB" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" style="height: 60px !important;width: 217px !important;" ></a>

This project is based on the ESP32, it works with the Adafruit Anemometer https://www.adafruit.com/?q=anemometer&

|<a href="https://www.aliexpress.com/item/32826540261.html?src=google&src=google&albch=shopping&acnt=494-037-6276&isdl=y&slnk=&plac=&mtctp=&albbt=Google_7_shopping&aff_platform=google&aff_short_key=UneMJZVf&&albagn=888888&albcp=7386552844&albag=80241711349&trgt=743612850714&crea=en32826540261&netw=u&device=c&albpg=743612850714&albpd=en32826540261&gclid=Cj0KCQjw-r71BRDuARIsAB7i_QMqV6A_E4zdDcSiXs2j3qIUm4cIgdCFfkDs1Egmak4QgCXrvfcQXAkaAu2WEALw_wcB&gclsrc=aw.ds"> ESP32 Dev Module</a>|<img src="./Pictures/ESP32.png" width="120"/>|
|---|---|
|<a href="https://www.adafruit.com/?q=anemometer&"> Adafruit Anemometer </a>|<img src="https://www.adafruit.com/images/1200x900/1733-00.jpg" width="120"/>|

<p align="center">
  <img src="./Pictures/AnemometerWebPage.PNG" width="350"/>
</p>

## Wiring

Device Pin | ESP32 |
--- | --- |
Anemometer blue (signal) | A0 |
Anemometer brown (+5) | Vin |
Anemometer black (Gnd) | Gnd |

<p align="center">
  <img src="./Pictures/ESP32%20Pinout.PNG" width="800"/>
</p>

Used the following development tools;

<ul>
  <li>Visual Studio Code with the PlatformIO extension.</li>
  <li>***** Don't forget to upload the index.htm file for the initial setup of the ESP32, Run 'Upload File System Image' platformio command</li>
  <li>Setup WIFI configuration by logging onto the "Anemometer" Access Point, use "Anemometer" as the AP password. Browse 192.168.4.1 and go to Configuration Page to provide your WIFI credentials.
</ul>

