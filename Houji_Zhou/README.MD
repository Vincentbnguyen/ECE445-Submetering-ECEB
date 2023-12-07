# Houji's Worklog
  - [2023-09-26 - Discussion with Professor Schuh and Professor Mironenko](#2023-09-26---Discussion-with-Professor-Schuh-and-Professor-Mironenko)
  - [2023-09-28 - First ESP32 trail](#2023-09-28---First-ESP32-trail)
  - [2023-10-03 - SD module delivered](#2023-10-03---SD-module-delivered)
  - [2023-10-10 - First version of pcb](#2023-10-10---First-version-of-pcb)
  - [2023-10-19 - Meeting with Jason](#2023-10-19---Meeting-with-Jason)
  - [2023-10-30 - Adjust the PCB design](#2023-10-30---Adjust-the-PCB-design)
  - [2023-11-01 - Finalize the PCB design](#2023-11-01---Finalize-the-PCB-design)
  - [2023-11-06 - Soldering practice](#2023-11-06---Soldering-practice)
  - [2023-11-15 - Visualize our design](#2023-11-15---Visualize-our-design)
  - [2023-11-18 - Execute test with Sophia on sensor circuits](#2023-11-18---Execute-test-with-Sophia-on-sensor-circuits)
  - [2023-11-19 - Continue testing](#2023-11-19---Continue-testing)
  - [2023-11-27 - Combine subsystems](#2023-11-27---Combine-subsystems)
  - [2023-11-28 - Prepare for the final demo](#2023-11-28---Prepare-for-the-final-demo)
  


# 2023-09-26 - Discussion with Professor Schuh and Professor Mironenko
We dicussed our project and ideas with professor Schuh and professor Mironenko. Professor Mironenko suggested us to implement the sensor circuits with transformers instead of hall effect sensor. She also emphasized that we do need to contact with the mechine shop to get more information about how to make our project work in the real world.
# 2023-09-28 - First ESP32 trail
Vincent gave me one of his esp32 ordered. I took home, connect it to my PC and loaded some simple program on it.
# 2023-10-03 - SD module delivered
My SD module and SD cards get delivered, I load some simple programs into esp32 to try to talk with SD card module. However, it can connect to the SD card but unable to write or read files inside of the SD card. Invesgating the reason.
# 2023-10-10 - First version of PCB
This week I constructed the first version of PCB according to Sophia's sensor circuit. And I use linear regulator as the core components to drop down the working voltage down to 3.3V. Here is the schematic view of our first version of PCB:
The sensor circuit subsystem

<img width="450" alt="sensor circuit" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/blob/main/Houji_Zhou/version%201%20sensor.png">

The power supply subsystem

<img width="450" alt="power supply" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/blob/main/Houji_Zhou/version%201%20power%20supply.png">

The programming circuit subsystem

<img width="450" alt="programming circuit" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/blob/main/Houji_Zhou/version%201%20programming.png">

And here is the first version of our PCB's board view

<img width="450" alt="first version pcb board view" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/tree/main/Houji_Zhou">

# 2023-10-19---Meeting with Jason
This week, we meet with Jason, and he discussed about how we treat the strapping pin for the ESP32. Also, he suggested us to follow the course wiki to make the programming circuit. In this case, we don't have to push buttons every time we want to load program to the ESP32 to make the ESP32 into download mode.

# 2023-10-30---Adjust the PCB design
At this point, We adjust the PCB design and updating version of our PCB. Firstly, we found that the linear regulator is not suitable for our power system. Since the battery we chosen is a 7.4v battery to prevent working voltage drop down under 3.3v. Since we need to make sure our system wouldn't stop working under the building will "hypothetically shut down" in a 24 hour period. However, the linear regulator couldn't handle drop down from 7.4v to 3.3v and will produce significant heats which can burn the regulator instantly. Thus, we adjust our power supply subsystem by using the buck convertor to handle the voltage drop down. Secondly, I use the programming circuit from the course wiki to simplify the programming steps. So the ESP32 can talk to computer without pushing buttons.
# 2023-11-01---Finalize the PCB design
I finalize the PCB design. Here is the schematic view of our first version of PCB:
The sensor circuit subsystem

<img width="450" alt="sensor circuit" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/blob/main/Houji_Zhou/snesor.webp">

The power supply subsystem

<img width="450" alt="power supply" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/blob/main/Houji_Zhou/power%20supply.webp">

The programming circuit subsystem

<img width="450" alt="programming circuit" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/blob/main/Houji_Zhou/programming.webp">

And here is the first version of our PCB's board view

<img width="450" alt="first version pcb board view" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/blob/main/Houji_Zhou/final_pcb.webp">

# 2023-11-06---Soldering practice

We use the old version PCB and extra components to practice soldering components on PCB.

<img width="450" alt="first version pcb board view" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/blob/main/Houji_Zhou/soldering_practice.webp">

# 2023-11-15---Visualize our design

We try to visualize our design that can put everything inside of the box, and make it can fit into the electrical room.

<img width="450" alt="first version pcb board view" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/blob/main/Houji_Zhou/virtualize%20design.webp">

# 2023-11-18---Execute test with Sophia on sensor circuits
I and Sophia go to the power lab and performe a serial of test on our sensor circuits. The test result shows the voltage sensor circuit can drop down the voltage that comes from voltage transformer to the acceptable range.Here are some testing figures and reading from O-scope.

<img width="450" alt="first version pcb board view" src="https://raw.githubusercontent.com/Vincentbnguyen/ECE445-Submetering-ECEB/main/Houji_Zhou/tesing.webp">

And here is the reading from O-scope

<img width="450" alt="first version pcb board view" src="https://raw.githubusercontent.com/Vincentbnguyen/ECE445-Submetering-ECEB/main/Houji_Zhou/test_result.webp">

#2023-11-19---Continue testing
Me and Sophia done more testing on the current sensor circuits.

<img width="450" alt="first version pcb board view" src="https://raw.githubusercontent.com/Vincentbnguyen/ECE445-Submetering-ECEB/main/Houji_Zhou/testing1.webp">

#2023-11-27---Combine subsystems
We meet up and try to combine our subsystems. We found that the stencil we ordered didn't come, and it is really hard to hand soldering the buck convertor and ESP32 on board. Here is the close pictures for the soldering.

<img width="450" alt="first version pcb board view" src="https://raw.githubusercontent.com/Vincentbnguyen/ECE445-Submetering-ECEB/main/Houji_Zhou/hand_soldering2.webp">

<img width="450" alt="first version pcb board view" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/blob/main/Houji_Zhou/hand_soldering1.webp">

And here is the view we combined all our subsystems.

<img width="450" alt="first version pcb board view" src="https://raw.githubusercontent.com/Vincentbnguyen/ECE445-Submetering-ECEB/main/Houji_Zhou/combined_subsystem.webp">

Also, we found the programming circuit didn't work on board. Guessing it is because we didn't groud all components properly. Thus we decide to use the dev board to demo our project. 
#2023-11-28---Prepare for the final demo
This is view of our final demo

<img width="450" alt="first version pcb board view" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/blob/main/Houji_Zhou/Final%20demo%20view.jpg">


