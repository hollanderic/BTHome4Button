This project will use a PCA10040 for development, and a custom PCB for the final product.

The device will be powered by a coin cell battery (CR2032).

The device will have four buttons for user interaction.

The device will have a single LED for status indication.

The device will implement the BTHome Button service for each of the four buttons, allowing for detection of single and double presses, as well as long presses.

Each button will be independent of the others.  

The device should use the lowest power mode possible to maximize battery life.  This will likely involve using the low power radio mode and sleep modes of the nRF52832.

Also support a second board for the project which is a PicoMicro NRF52840.  On that board buttons 1-4 will be connected to pins 17, 20, 22, and 24 respectively.  The inputs should use internal pull downs and will be active high.  Progamming will be done via the USB connector.