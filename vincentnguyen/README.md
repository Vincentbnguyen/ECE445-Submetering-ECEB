# Vincent Worklog
  - [2023-09-24 - ESP32 Received](#2023-09-24---esp32-received)
  - [2023-09-26 - Discussion with Professor Schuh and Professor Mironenko](#2023-09-26---discussion-with-professor-schuh-and-professor-mironenko)
  - [2023-09-28 - Power Calculation Lessons](#2023-09-28---power-calculation-lessons)
  - [2023-10-2 - Successful Data Upload](#2023-10-2---successful-data-upload)
  - [2023-10-4 - ADS115 Discovery](#2023-10-4---ads115-discovery)
  - [2023-10-10 - Simulating Sinusoidal Voltage and Current Inputs](#2023-10-10---simulating-sinusoidal-voltage-and-current-inputs)
  - [2023-10-11 - Calculating Real and Apparent Power](#2023-10-11---calculating-real-and-apparent-power)
  - [2023-10-31 - Using Scopy for waveforms input](#2023-10-31-using-scopy-for-waveforms-input)
  - [2023-11-2 - Realized there is a sampling delay between voltage and current input](#2023-11-2-realized-there-is-a-sampling-delay-between-voltage-and-current-input)
  - [2023-11-6 - PCB Soldering](#2023-11-6-pcb-soldering)
  - [2023-11-13 - Design + 3D Print Enclosure](#2023-11-13-design--3d-print-enclosure)
  - [2023-11-27 - Finalizing my part and finalizing code and observing accuracy](#2023-11-27-finalizing-my-part-and-finalizing-code-and-observing-accuracy)
  - [2023-12-4 - Finalize for Demo](#2023-12-4-finalize-for-demo)

# 2023-09-24 - ESP32 Received
My ESP32 order got delivered and I played around with uploading and running some code on the device and trying to connect to my apartment WiFi,

# 2023-09-26 - Discussion with Professor Schuh and Professor Mironenko
We discussed the project requirements and bounced some ideas around on the method for our voltage and current measurements, and our vision for the end project.


# 2023-09-28 - Power Calculation Lessons
We discussed the concept of sampling and calculating the real and apparent power given a sinusoidal voltage input into the microcontroller and came up with a plan on how to implement that.

<img width="450" alt="image" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/assets/90225857/fab6eee2-ae23-419f-9cf9-4210f8ffabdd">

# 2023-10-2 - Successful Data Upload
I successfully was able to create a IoT Hub instance on Azure, and upload mock data to that hub. This was made possible by a Arduino Library offered by Azure where it utilized the MQTT protocol to connect my ESP32 to the IoT Hub and send a JSON Payload.
I then routed that data from the IoT Hub to an Azure Cosmos Database.
I also confirmed that I was able to provide the ESP32 a WiFi connection on Illinois_Guest by registering the device with its MAC Address on this site: https://clearpasspub.techservices.illinois.edu/guest/auth_login.php
This is great since ideally the device should be able to perform at the ECEB!

<img width="1908" alt="Screenshot 2023-10-11 at 1 34 48 AM" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/assets/90225857/95262fd5-a486-4533-b27b-dd4d563cdd55">

# 2023-10-4 - ADS115 Discovery
I researched more into Analog-to-Digital converters onboard the ESP32 microcontroller and realized we wanted more accuracy, so an ADS1115 circuit is a better option to have more reliable and consistent analog inputs.

# 2023-10-10 - Simulating Sinusoidal Voltage and Current Inputs

<img width="1920" alt="image" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/assets/90225857/9b5cb780-ec59-48be-88b2-ea6277236a7c">

While our sensors were being built, I prepared our program to mock voltage and current inputs so that I can start building the functionality to sample the input. I created a sin function and displayed it on the Arduini IDE's serial monitor to see the created waves.

# 2023-10-11 - Calculating Real and Apparent Power

<img width="1920" alt="Screenshot 2023-10-11 at 1 28 50 AM" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/assets/90225857/66f65379-7b41-402c-b304-ebc9ef43d081">

I sampled my simulated inputs and created a program that uses a rolling window of samples to calculate and update the real and apparent power. Right now, it currently is able to sample at 1000 HZ of my simulated wave and produce real and apparent power claculations.

Here is the current method for power calculations:
To calculate real power:
For every sample, we multiply the instantaneous voltage and current. Once we have enough samples, we then average this sample set to produce our real power. In order to update our real power as we continue to sample, we remove the oldest sample in our sample set and factor in the new sample.

To calculate apparent power:
For every sample, we process voltage and and current seperately where we square each measurement. Once we have enough samples, we take the average of each sample set, and then square root them to obtain the RMS value for each sample set. The apparent power is then calculated by the multiplying the voltage and current RMS values. In order to update our apparent power as we continue to sample, we remove the oldest samples in our sample sets, and factor in the new samples.

# 2023-10-31: Using Scopy for waveforms input
I was able to generate sample waveform inputs at home using Scopy with set parameters for easy testing.
![IMG_5293](https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/assets/90225857/474ec63f-042c-44fd-9b30-4c441aee007c)

<img width="1918" alt="Screenshot 2023-10-31 at 9 43 26 PM" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/assets/90225857/cc82c02a-789d-4950-addd-418a7ad9450c">
<img width="1920" alt="Screenshot 2023-10-31 at 9 43 36 PM" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/assets/90225857/6fef086e-24ba-4451-bbb9-c00bb0636e1e">

# 2023-11-2: Realized there is a sampling delay between voltage and current input
There is a small delay between sampling voltage and current input, so I did some math to manually add some phase offset to account for this.
<img width="820" alt="image" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/assets/90225857/a53c0221-1d90-4cab-b4e8-c02dc469e98b">


# 2023-11-6: PCB Soldering
We practiced soldering the componenets on our first version of the PCB.
![IMG_5187](https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/assets/90225857/cf9c103b-18c2-48d0-a292-2e90b902bd3b)

# 2023-11-13: Design + 3D Print Enclosure
I made a modular design that I could transport from my home onto the plane and the pieces were superglued together.
![image](https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/assets/90225857/4be10618-cc8b-4dfe-89a7-64fa1d5e2317)

# 2023-11-27: Finalizing my part and finalizing code and observing accuracy
I used a new ADC that samples faster and started assembling the 3D print.
![IMG_5414](https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/assets/90225857/a585ad43-cf45-4afb-8edc-12980f6d30e8)
![IMG_5415](https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/assets/90225857/285453a4-3356-4e18-b116-d2f87e7fea33)
Results with low power sample input:
<img width="1410" alt="image" src="https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/assets/90225857/5865a38c-ef35-46fd-a110-e247d8c8b9ff">


# 2023-12-4: Finalize for Demo
Parts seemed to fit inside enclosure and my signal processing code accurately measures low power waveform inputs.
![IMG_5487](https://github.com/Vincentbnguyen/ECE445-Submetering-ECEB/assets/90225857/5b1bb101-4e2c-4ba6-9165-d4d6427da7f1)




