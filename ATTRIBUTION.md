# Attribution

## Educational and provenance notice

This repository contains an ESP32 firmware port for the ELEGOO Tumbller
V1.1 self-balancing robot.

**Commercial use is not permitted.** The work is intended exclusively for
educational, personal and non-commercial learning purposes. Commercial use,
including use in commercial products, services or distribution, requires
separate written permission from the relevant rights holders.

## Original source

The underlying ELEGOO Tumbller source is provided by ELEGOO:
github.com/elegooofficial/ELEGOO-TumbllerV1.1-Self-Balancing-Car-Tutorial
- https://
- Official source package: `Tumbller_Code_20191012.zip`

The official ELEGOO repository does not contain a separate `LICENSE` file.
This attribution documents provenance; it does not represent that a license
has been granted or that commercial use is permitted.

## OE3KUW source and this port

The `master` firmware in this repository is based on:

- https://github.com/OE3KUW/TumbllerVSC

The OE3KUW project provides the Arduino Uno / ATmega328 Tumbller firmware and
hardware documentation used as an intermediate source for this ESP32 port.
The ESP32 firmware retains the OE3KUW control structure and tuning wherever
possible. ESP32-specific hardware and software changes are identified in the
source comments.

The complete source chain is:

```text
ELEGOO official Tumbller V1.1 source
        |
        v
OE3KUW/TumbllerVSC
Arduino Uno / ATmega328 port and documentation
        |
        v
This ESP32 / tumlerV2 port
```

The `OE3KUW/TumbllerVSC` repository has no license file. Source attribution
therefore preserves provenance, but does not provide a usage license or
permission beyond upstream rights.

## Source mapping

| File or component | Relationship |
|---|---|
| `src/main.cpp` | Derived from the OE3KUW/TumbllerVSC control flow and ELEGOO Tumbller structure |
| `src/BalanceCar.h` | Derived from the OE3KUW/TumbllerVSC balancing, speed and steering control |
| `src/KalmanFilter.h`, `src/KalmanFilter.cpp` | Derived from the OE3KUW/TumbllerVSC filter implementation |
| `src/MPU6050.h`, `src/MPU6050.cpp` | MPU6050 implementation supplied through the OE3KUW/TumbllerVSC source |
| `src/I2Cdev.h`, `src/I2Cdev.cpp` | I2Cdev implementation supplied through the OE3KUW/TumbllerVSC source |
| `src/helper_3dmath.h` | 3D math helper supplied through the OE3KUW/TumbllerVSC source |
| `src/Pins.h` | ATmega328 pin definitions adapted to the `tumlerV2` ESP32 board |
| `src/mode.h` | Operating-mode naming and structure derived from OE3KUW/TumbllerVSC |
| `src/Rgb.h` | Status and RGB method structure derived from OE3KUW/TumbllerVSC |
| `src/Ota.h`, `src/Console.h`, `src/WebUi.h`, `src/Bt.h` | ESP32-specific additions for OTA, console, web and Bluetooth functionality |

## Third-party source notices

The `I2Cdev`, MPU6050 and `helper_3dmath` files retain their upstream copyright
and license notices. In particular, `I2Cdev.cpp` and `helper_3dmath.h` contain
MIT license text for Jeff Rowberg's i2cdevlib code. Those notices must remain
with the corresponding source files.

Other third-party libraries used by this project remain subject to their own
licenses. Their license text must be retained when their source or package is
redistributed.

=======================================================================================================================
26.11.2026:
Dear Sir or Madam,
 
I am a teacher at a technical secondary school and am interested in your product, Tumbller.
 
While researching, I found the following software developed by your company on GitHub: github.com/elegooofficial/ELEGOO-TumbllerV1.1-Self-Balancing-Car-Tutorial
 
I would like to ask whether I may modify and extend this software with my own ideas and use it exclusively for educational purposes in my classes. Furthermore, would it be permissible to share the software, including any modifications, with fellow teachers and students?
 
Please note that there will be no commercial use of the software under any circumstances. Its use would be strictly limited to teaching and educational activities.
 
Thank you for your time and consideration. I look forward to your response.
 
Kind regards,
 
Wolfgang Kuran
Teacher
=======================================================================================================================
27.11.2026:
Dear Customer,

Thank you very much for your inquiry and your interest in our Tumbller product. This is Fiona from the Elegoo Support Team. 

You are welcome to modify, extend the open-source code from the GitHub repository, and use it purely for your in-class educational purposes. It is also permitted to share the original code as well as your modified versions with your fellow teachers and students.

Please kindly note that this permission is granted on the condition that there is no commercial use of the code and derivatives in any form.

We are glad our open-source project can support your technical secondary school teaching. If you have any other technical questions when using the code, feel free to let us know.

Best regards,
Fiona
Elegoo Support Team
Please let us know how we did. Were you satisfied with the support provided?

=======================================================================================================================


