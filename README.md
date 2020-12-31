# PrusaPager

I wanted to be alerted when my prints had finished on my Prusa MINI 3d printer.

What better way then a pager; OK a smart phone App!

The PrusaConnect Local web page is great for tracking the status of prints, but does not alert you when the print in progress finishes. But it does provide an JSON API with the printer status.

I went for the retro approach with an '80 solution and created an old fashioned Pager.

The PrusaPager uses an ESP8266 Wireless module with an oled display and sounder. It calls the PrusaConnect Local API to get the printer status, which is displayed on the oled display. Triggering the sounder when the print has finished.

I used a Heltec WiFi 8 as they are ready available at very low cost. The code should be compatible with other similar boards.

Full details and stl files for the 3d printed parts are available at: https://www.prusaprinters.org/prints/50322-prusapager-mini-mini
