# shellyEM mockup
Base code for shelly EM energy meter


Shelly EM is a impressive mini device that comes with the following features:

- I2C RTC based on PCF85363A
- I2C energy meter based on AD7953
- Relay
- Button
- LED
- Esp 8266EX chip with 4Mb of flash

This code serves as a buliding block to develop whatever you want on top of this great device.


Check out the manufacturer, [shelly](https://shelly.cloud/shelly-energy-meter-with-contactor-control-wifi-smart-home-automation/) for features and hardware details.


## Disclaimer & instructions:
  - AC mains can **kill** you. You wont be able to undo any mistake after you're **dead**.
  - You **CANNOT** connect shelly to mains AC and to the programming pins at the same time.
  - Disconnect the device from any mains before program it with this firmware
  - provide your own WiFi, MQTT and Emoncms configuration and upload the firmware as a starting point. 
  - Check serial UART for IP address, check if Emoncms and MQTT is working, disable the features you don't want on the code. 
  - After everything is tested disconnect the programming pins and connect to AC mains and test again if the device is working properly. 
  - From there you can change the code and do OTA updates to the ESP8266 using the url: `http://<ip_of_device>/update`
  
  
![](https://shelly.cloud/wp-content/uploads/2019/04/pin_out_gpio-650x397.png)


This code has the following features:

  - press button for 2 seconds to toggle relay.
  - LED follows Relay status
  - Communcate with ADE7953 and PCF85363A via I2C
  - Publish energy readings to emoncms server every 10s
  - Publish RTC time and energy readings to MQTT every 1s
  - WebUpdater.ino code included for easy updates OTA - http://<IP_address>/update
  
  ![](https://lh3.googleusercontent.com/pw/ACtC-3euHPy76cJqumeQIC3ssVWz0JF5_0skSKkTX7I6qG9k7677l6aEp3UTEn6t9fXDrYqIl5KPrMfg3z3qBLNKoAsCKRHcWfyogfXBR57wNyaMJBa-rBTmf4WIbkGp633juPkwd-q1MW1Dp9LgECpZ5xOR=w892-h437-no?authuser=0)

  
  Have fun!
  
 If you find any improvements, like better calibration procedure or find any problems please open a issue.
