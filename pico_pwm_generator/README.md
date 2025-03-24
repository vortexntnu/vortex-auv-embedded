# PWM GENERATOR ON ORCA
Firmware for the AUV to control PWM output and set I2C communication with the rest of the system.

# How to Use: 
This firmware utilises the official SDK for the Raspberry Pi Pico to listen on the I2C bus for PWM values which will then have to be written into their corresponding ESC. The I2C address for the Pico is '0x21' and can be changed from the i2c.h files. 
 
 ## Prerequisites 
 Before using this code, ensure that you have completed the following: 
 1. **Installed and configured the Pico SDK:**
   Can either be done through installing the pico sdk itself and configuring it to path and all that. Or you can use a vscode extension that can do most of the tedious work for you. I recommend the latter as the only thing you would be missing on is configuinrg CMakeLists and cmake (yaay fun stuff)...
   As this is being written at 0307, I will opt for the easy explanation, that being vscode extension:

      1. **Install Dependencies**
      ```bash
      sudo apt install python3 git tar build-essential
      ```
      2. **Install the Extension**
      Extension is called "Raspberry Pi Pico" which you can find by searching for it in the extensions. 
      You can also find the store entry at  https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico.
      Or if you prefer, here is the source code for the extension:  https://github.com/raspberrypi/pico-vscode.
      Check activity sidebar after installation as it should now contain a new section with a Raspberry Pi icon. 

Installing this extension could take a bit as it will try to check if you have the sdk and if it is configured and will install and configure it if you dont have it. After that, you should be able to see some new capabilities added to the low sidebar of vscode on the right side where one of them says "Compile". If you click on that, a new directory should be made, called build, and inside there you can find a file called "pic_pwm_generator.uf2". To run this on the pico you will have to connect the pico to the pc in "Bootsel" mode, which can be done if you hold the Bootsel button on the board before you plug it into the pc and release it after a while of plugging it. The confirmation for entering Bootsel mode would be that the pico should be mounted as another directory that you can access from you sidebar on Ubuntu or system files on other OS's. From there, the only thing to do is to drag and drop the .uf2 file into the pico. 

**NOTE** Make sure that the pico is unplugged from the rest of the system before attempting to connect it to your pc. 

The code defines three states of "operation" that defines what the built in LED blinking sequence should be, those being: 
1. During startup, the LED will blink rapidly for 20 iteration which each iteration being 100ms long. 
2. During Idle state, the LED will have a slow blink sequence which would toggle it every 500ms 
3. After receiving any successful I2C message, the LED should blink for 5 iterations, which each being 20ms long. 

This is done to ensure a visual representation of what state the pico is operating at when access to the board itself and the i2c buss is limited or not possible. To alter or disable this sequence, refer to the function called "led_sequence" in main.c

The code also utilises two modes of operation for the I2C, that being one with internal pullups enabled and one without. To enable or disable the internal pullups, uncomment 3rd line and comment the 2nd or vise versa in main.c respectively. Make sure to have PULLUP_DISABLE defined as the default state of operation, that is if neither PULLUP_ENABLE nor PULLUP_DISABLE are defined, is to run with PULLUP_ENABLE. To change that, refer to i2c.h. 

The code should be combatible with C++ and can be included in any C++ project. 

Written by Ibrahim F. Abdalaal. God speed and if this crashes, then it is surely an elecrical problem.