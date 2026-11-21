# Kinetic Kitchen Technical Report

**Expo Ingenierías 2026**

**Tecnológico de Monterrey, Campus Monterrey**
**School of Engineering and Sciences**
**FrED Factory Lab (Tecnológico de Monterrey and MIT collaboration)**

| Category | Desarrollo de Prototipo Físico |
| :--- | :--- |
| Attribute | Emprendimiento |
| Team | Cecil Yusei Howell (project manager), Carlos Alberto Garcia Leyva, Yoana Montoya Garza, Javier Taddeus Hernández Garza. |
| Advisor | Dr. Erick Ramírez-Cedillo, Program Director, Mechatronics Engineering |
| FrED Factory project lead | Bárbara Alejandra Novau Martínez |
| Lab | FrED Factory |
| Built (prototype) | 65 cm × 20 cm × 10 cm conveyor, 3 start positions, 5 collection boxes |
| Planned (full station) | A longer conveyor in front of 3 Bambu Lab P1S printers, with 12 boxes to hold one day of prints |
| Period | Fall 2026 (Expo Ingenierías, December 1, 2026) |

---

## 1. Abstract

Print farms already automate job queues and bed clearing, but finished parts still fall into one shared bin. Parts hit each other, orders from different students get mixed, and nobody knows who picked up what. Kinetic Kitchen is a sorting station that sits in front of a row of 3D printers and sends each finished part to its own box. The full station is designed for three Bambu Lab P1S printers: a longer and wider conveyor, 12 boxes to hold one day of prints, a load cell under each box, and a message to the student with a photo of the part and the box number. This semester the team built a small prototype to prove the routing. It is a 65 cm conveyor made from laser-cut MDF, 3D printed rollers, a NEMA 17 stepper motor, and an ESP32. It has three start positions, which stand in for the three printers, and five boxes. Two boxes are at the ends of the belt, and the belt reaches them by running forward or backward. Three boxes are along the side, and a servo arm set at 65° to the belt pushes the part off the belt into each one. Two boxes sit on load cells, so the system knows when a box is occupied and when it is vacant again. A web page lets the user choose a start position and a box and shows box status. In 252 test runs with 24 printed parts, the prototype delivered 94.8% of parts to the chosen box, every route finished in under 8 s, and the load cells read within ±2.75 g of a lab scale. Ten of the 13 failures were parts that reached the right box but did not fit fully inside it.

**Keywords:** 3D print farm, part sorting, conveyor, stepper motor, load cell, ESP32, Bambu Lab P1S, FrED Factory.

---

## 2. Introduction

Fast desktop printers and automatic bed clearing let a small lab run prints back to back with nobody present. In the FrED Factory Lab, the B.L.A.Z.E. team (Bambu Lab Automated Zero-Touch Executor) is building a system where students upload files to an online queue and the printers remove finished parts on their own. The physical side after that is not solved. Once a part is pushed off the bed, it lands in a bin with everyone else's parts.

Kinetic Kitchen covers that last step. It takes the part after it leaves the printer, moves it to a box assigned to that job, and records that the box is taken until someone removes the part. The goal is for picking up a print to work like picking up a takeout order.

---

## 3. General Context

FrED stands for Fiber Extrusion Device, a low-cost desktop machine that MIT designed to teach manufacturing. The FrED Factory at MIT is a small production facility that builds these devices and is also used for teaching and research (MIT FrED Factory, n.d.). MIT and Tec de Monterrey run the program together, and FrED factories now operate at MIT and at Tec's Monterrey and Mexico City campuses (DiCristofaro, 2026).

Kinetic Kitchen is a student project in the FrED Factory Lab at Campus Monterrey. It was accepted under the lab's research area "Building the Task Force of the Future", which looks at new physical and intelligent systems for manufacturing (FrED Factory Lab, n.d.).

The lab runs Bambu Lab P1S printers that many students share. B.L.A.Z.E., another project in the lab, automates what happens between prints (B.L.A.Z.E. team, 2026a, 2026b):

* **Hardware.** A door actuator, a build plate actuator, and a pusher on the extruder remove the part. The printer sits on inclined feet. An infrared sensor confirms that the bed is clear before the next job.
* **Control.** A Raspberry Pi 5 coordinates the printers over MQTT without modifying them.
* **Web queue.** Students log in with a tec.mx account and upload a file. The system stores who sent each job, which printer ran it, and when it finished, and it shows notifications in the app.

B.L.A.Z.E. ends when the part leaves the printer. Kinetic Kitchen picks up from there.

Shared labs like this are common. The Fab Foundation alone lists more than 1,500 fab labs in over 90 countries (Fab Foundation, n.d.), and that count does not include university makerspaces outside its network. These labs need a handoff system that is cheap, safe to have around students, and easy to maintain. Industrial sorting equipment does not fit that description.

---

## 4. Scope

The project has two parts: the full station, which is the actual product, and the prototype, which is what was built this semester. The rest of this report labels each section or number as one or the other.

| | Built: prototype (Fall 2026) | Planned: full station (2027 to 2028) |
| :--- | :--- | :--- |
| Purpose | Prove the routing and the box sensing | Serve the FrED Factory printers every day |
| Printers | None connected. 3 start positions stand in for them. | 3 Bambu Lab P1S (389 mm each, about 1.25 m span), with B.L.A.Z.E. installed |
| Conveyor | 65 cm long, 10 cm wide belt | About 2.8 m long, 30 cm wide belt (section 15.2) |
| Boxes | 5 | 12, enough for one day of prints |
| Weight sensing | Load cells under 2 boxes, used to tell occupied from vacant | Load cell under every box, also checked against the slicer weight |
| Arms | 3, SG90 servos, 14 cm | 10, MG996R servos with a tip stop, about 33 cm |
| Part weight | 10 g to 250 g | 10 g to 500 g |
| How a route starts | User picks start and box on a web page | "Bed cleared" signal from B.L.A.Z.E. |
| How parts get on the belt | Placed by hand | Ejected by the printer (B.L.A.Z.E. team) |
| Photo and message to the student | No | Yes |
| Frame | 5.5 mm MDF, chosen because it is cheap and fast to cut | Metal |

Weight sensing is in scope for both. It is how the system knows whether a box is occupied or vacant.

Out of scope for both versions this year: locked boxes and user ID at pickup.

**Why the prototype is smaller.** Three P1S printers side by side span about 1.25 m, and the full station also needs room for 12 boxes and a belt wide enough to catch falling parts. The prototype is a cheap, small version with a 10 cm belt. It tests routing and sensing but not the catch. Section 7.5 shows that the same design holds at full size.

---

## 5. Problem Statement

When several users share printers with automatic ejection and one drop bin, four problems show up:

1. **Damage.** Parts fall onto earlier parts. Thin walls crack and surfaces get scratched.
2. **Mixed orders.** Students dig through a pile to find the right print, and sometimes take the wrong one.
3. **No record.** Nothing confirms that a part reached a bin or when it was picked up.
4. **Cost of the alternatives.** A robot arm can do this job, but a new UR5e costs about $30,000 to $45,000 USD, and a full cell with gripper and setup often costs $40,000 to $150,000 USD (Standard Bots, 2024).

### 5.1 Research Questions

**Main question:** Can a small conveyor with 65° servo arms and a reversible belt send a part from any start position to any assigned box reliably?

**Sub-questions:**

1. Do the 65° arms push parts of different shapes and weights off the belt without jamming?
2. How long does each route take?
3. Do the load cells correctly show when a box is taken and when it is free again?
4. How much weight can the belt and the arms handle?

---

## 6. Justification

1. **Five boxes with three servos.** The belt runs both ways, so the two end boxes need no arm. A part just rides off the end. Only the three side boxes need a servo.
2. **No optical sensors on the belt.** Infrared sensors give false readings with room light, dust, and translucent filament. The start of each route comes from a command, and arrival is confirmed by weight.
3. **Pushing instead of gripping.** Printed parts come in any shape. An arm that pushes does not need to know the shape. A gripper does.
4. **Motor off between prints.** Prints take hours. The stepper driver is disabled between jobs so the motor draws no current while waiting.
5. **Cheap, common parts.** MDF, printed PLA, a stepper, hobby servos, and an ESP32. A lab can repair it with what it already has.

---

## 7. Theoretical Framework

Conveyors that divert items with an angled arm are common in packaging and warehouse lines. Hobby versions with a stepper motor and a microcontroller are common too ("How to make a DIY belt conveyor," 2022). A robot arm can also sort printed parts, but it costs far more (section 5). Printed parts come in any shape, so a fixed angled arm that pushes the part is the simplest option. It does not need to grip anything.

Most numbers in this section are calculated from the design, not measured. Values marked "assumed" need to be checked on the prototype. Measured values are in section 14.

### 7.1 Arm Deflection

The belt moves at speed v. The arm sits at θ = 65° to the belt direction. A part that reaches the arm slides along it while the belt keeps pushing it forward. With a friction coefficient μ between the part and the arm, the speed along the arm is:

v_arm = v (cos θ − μ sin θ)

The sideways part of that speed is:

v_side = v_arm sin θ = v sin θ cos θ (1 − μ tan θ)

The arms were first designed for 45°. At 45° the 140 mm arm only reaches 99 mm across the 100 mm belt, and in bring-up 45° and then 55° did not push parts off reliably, so the arms were set to 65° before the counted test runs. The servo turns to 65° or to 115° (the mirror image), depending on which way the belt runs, so the arm always meets the part at 65°.

With v = 96.4 mm/s (the measured belt speed, 7.2) and μ ≈ 0.25 (assumed):

| Quantity | Value |
| :--- | :--- |
| Arm length | 140 mm |
| Belt width the arm covers at 65° | 127 mm (the belt is 100 mm) |
| Sideways speed of the part | 17 mm/s |
| Time to cross half the belt (part placed at the center, 50 mm) | 2.9 s |
| Time to cross the whole belt (100 mm) | 5.8 s |
| Angle where the part stops sliding (tan θ = 1/μ) | 76° |

The calculated 2.9 s matches the test: start 1 to box 2 needs no belt travel, and it took 3.3 s on average, which is 0.5 s for the arm to swing out plus about 2.8 s of crossing. That puts the real friction close to the assumed 0.25. At 65° the arm is 11° below the 76° limit, so most parts slide, but slowly, and a part with more friction can stop on the arm. That matches part 12, a small cube, which got stuck twice (14.3).

**Servo check.** The belt pushes the part against the arm with at most the belt-to-part friction force. With a belt friction coefficient of 0.5 (assumed), the worst case is the part pressing on the arm tip, 140 mm from the servo.

| Part mass | Torque needed at the tip | SG90 (about 0.18 N·m) | MG996R (about 0.92 N·m) |
| :--- | :--- | :--- | :--- |
| 250 g | 0.17 N·m | At its limit | Safety factor 5.4 |
| 500 g | 0.34 N·m | Not enough | Safety factor 2.7 |

The prototype uses SG90 servos, which limits it to parts of about 250 g. The full station will use the MG996R servos the team already has.

### 7.2 Motor and Drive

The NEMA 17 (StepperOnline 17HS19-2004S1) is rated for 59 N·cm of holding torque at 2.0 A (StepperOnline, n.d.). Holding torque is the value at standstill, so it cannot be used as running torque. The driver gives it about 1.5 A per coil (below), so its holding torque is about 59 × 1.5 / 2.0 ≈ 44 N·cm, and a fair estimate for usable torque at low speed is about half of that, 22 N·cm. The motor is driven by an A4988 driver from the 12 V supply, in full-step mode, with the reference voltage set to 1.68 V. The A4988 sets its current limit as I = Vref / (8 × Rs), where Rs is the sense resistor on the driver board, and in full-step mode each coil gets about 71% of that limit (Allegro MicroSystems, n.d.). The driver board uses 0.1 Ω sense resistors (marked R100), so the limit is 1.68 / (8 × 0.1) = 2.1 A and each coil gets about 1.5 A in full-step mode. That is under the motor's 2.0 A rating.

The motor drives the end roller through a GT2 timing belt (2 mm pitch, 6 mm wide, 200 mm loop) with a 20-tooth pulley from the kit on the motor and a 40-tooth pulley printed in PLA on the roller rod, a 2:1 reduction. The kit's 36-tooth pulley was not used. The end rollers are 30 mm in diameter. The conveyor belt loop is 120 cm, which puts the end roller centers about 553 mm apart. At 150 mm/s the roller turns at about 95 RPM and the motor at about 191 RPM.

**Belt speed.** A stepper motor turns a fixed angle per step, so belt speed is set by the step rate the ESP32 sends, the pulley ratio, and the roller diameter. Motor torque does not set the speed. It only decides whether the motor can hold that speed under load, which is the check in the table below.

v = π × D × (f / (200 × m)) × (20 / 40)

Here D is the roller diameter (30 mm), f is the step rate in steps per second, 200 is the motor's steps per turn, and m is the microstep setting of the driver.

| Microstep setting | Step rate for 150 mm/s |
| :--- | :--- |
| Full step (m = 1) | 637 steps/s |
| 1/8 (m = 8) | 5,093 steps/s |
| 1/16 (m = 16) | 10,186 steps/s |

**Measured distance per step.** The belt moves farther than this formula says. A 200 mm move commanded with the formula moved the belt 245 mm, and four 50 mm moves moved it 222 mm in total. Both fit one rule: the belt moves 1.263 times the formula distance, minus about 7.7 mm lost to slack at the start of each move. The factor 1.263 means the roller acts like a 37.9 mm roller instead of 30 mm. The 2 mm belt on the outside of the roller explains only part of that, and the rest was not traced. The firmware now uses both measured values, so a move goes the distance asked for.

The driver runs in full-step mode (m = 1). With the measured factor, the belt speed of 96.4 mm/s (14.2) corresponds to about 324 steps/s, and the firmware is set to 95 mm/s, within 1.5% of the measured speed. That is 64% of the 150 mm/s design value. This is on purpose: the team lowered the step rate in the firmware to reduce belt slip. Before the change the belt ran at 131.3 mm/s (9.14 s per lap, about 441 steps/s), 88% of the design value, so the change cut speed by 26.6%.

| Step | Calculation | Result |
| :--- | :--- | :--- |
| Usable motor torque | 0.5 × 44 N·cm | 22 N·cm |
| Torque at roller | 22 × 2 × 0.95 (timing belt efficiency) | 41.8 N·cm |
| Belt pull | 41.8 N·cm / 1.5 cm | 27.9 N |
| Load to move | 0.65 kg (top run of belt plus a 500 g part) × 9.81 × 0.35 | 2.2 N |
| Margin | 27.9 / 2.2 | about 13 |

The load figure is on the safe side, because the belt rides on rollers and does not drag on a deck. The motor is not the limit. The more likely failure is the drive roller slipping on the belt, which depends on belt tension.

**Support rollers.** Two rollers sit between the end rollers so the belt does not sag under a part. They are 35 mm in diameter, 5 mm larger than the end rollers. With four equal rollers the belt did not touch the middle two. The larger middle rollers push up into the belt, which gives contact and the right tension.

**Belt tracking.** The first prototype used cone-shaped roller ends to center the belt. The belt slowly climbed the cones and drifted. The current rollers have flat flanges that are too tall for the belt to climb, and the belt now stays in place.

### 7.3 Box Sensing

Boxes 2 and 4 each sit on a 1 kg bar load cell. The load cell is a strain-gauge bridge read by an HX711 amplifier, which sends readings to the ESP32 over a clock line and a data line (Avia Semiconductor, n.d.). Noise limits the useful resolution to roughly 1 g on this type of cell. Because the cells are rated for 1 kg, 1 kg is the heaviest part the boxes can weigh. A part over 1 kg needs a 2 kg or 5 kg load cell.

In the prototype the load cells answer one question: is the box occupied or vacant?

* **Occupied:** the reading stays above 3 g for 1 s.
* **Vacant:** the reading stays below 2 g for 2 s. The box is re-zeroed each time it is marked vacant, because load cells drift with temperature. While a box is vacant the firmware also follows slow drift below 1.5 g.

During a route the belt stops as soon as the reading stays above 3 g for 0.3 s, then waits 1.5 s and checks the weight again. These thresholds were used in every test run, and box status was correct on all 52 runs where it was recorded (14.2), so they were kept.

In the full station every box has a load cell, and the measured weight will also be compared with the slicer's estimate to confirm that the right part arrived. The planned tolerance is the larger of 5 g or 5% of the estimate. A flat 5 g tolerance is too loose for a 10 g part and too tight for a 500 g part. The measured margin of ±2.75 g (14.2) fits inside this tolerance.

### 7.4 Timing

Calculated at the measured belt speed of 96.4 mm/s, with the part placed at the center of the belt and the arm at 65° (7.1). Route time runs from pressing Run until the part lands in the box, which is what was timed in the tests:

| Route | Arm out | Belt travel | Crossing | Calculated | Measured average (slowest) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| Start 1 to box 2 (right beside it) | 0.5 s | 0 s | 2.9 s | 3.4 s | 3.3 s (5.1 s) |
| Start 1 to box 4 (about 369 mm) | 0.5 s | 3.8 s | 2.9 s | 7.2 s | 6.0 s (6.5 s) |
| Start 1 to box 5 (off the far end, about 476 mm) | none | 4.9 s | none | 4.9 s | 5.4 s (5.6 s) |

The longest calculated route is about 7.2 s, and the slowest measured run was 7.63 s (average 4.0 s, 14.2). The target is 8 s for any route. The firmware stops the belt if a route takes more than 15 s. The limit was 10 s during the tests and was raised afterwards, when box 3 was set to run twice its planned distance (10.3). After the belt stops on boxes 2 and 4, the firmware waits another 1.5 s to confirm the weight. That check is not part of the route time.

### 7.5 Scaling to the Full Station

The same formulas were applied to the full station proposed in section 15.2: a conveyor about 2.8 m long, a 30 cm belt, 50 mm rollers, a 3:1 reduction, and MG996R servos. All numbers below are calculated.

| | Prototype (built) | Full station (planned) |
| :--- | :--- | :--- |
| Arm length to cover the belt at 65° | 140 mm | 331 mm |
| Heaviest part | 250 g | 500 g |
| Torque on the servo at the heaviest part, load at the arm tip | 0.17 N·m against 0.18 N·m (SG90) | 0.81 N·m against 0.92 N·m (MG996R) |
| Belt pull against load | 27.9 N against 2.2 N | 25.1 N against 9.2 N |
| Motor margin | about 13 | about 2.7 |
| Route to the nearest box (part at the belt center) | 3.4 s | 6.1 s |
| Longest route | 7.2 s | 17.4 s |
| Route time target and timeout | 8 s and 15 s | 20 s and 25 s |

The full station numbers use the 150 mm/s design speed. A part that lands at the far edge of the 30 cm belt takes about 5.6 s longer to cross than one at the center.

What this shows:

* **Servo.** The servo is the weak point in both versions. At full size the MG996R holds a 500 g part at the arm tip with a safety factor of only 1.1. A fixed stop at the arm tip, so the stop takes the push and not the servo, is needed.
* **Motor.** The same NEMA 17 still works, but the margin drops to about 2.7 with the cautious friction value used here. If testing shows less, the options are a larger motor or a lower belt speed.
* **Time.** At 65° the part crosses the belt slowly, so routes are longer, but a print takes hours, so 17 s to deliver it does not matter. The time only limits how fast parts can be cleared when several printers finish together.

---

## 8. Objectives

### 8.1 General Objective

Design, build, and test a conveyor prototype that sends a part from any of three start positions to any of five boxes and detects when a box is taken or free, by December 1, 2026.

### 8.2 Specific Objectives

1. **Mechanical.** Build a 65 cm × 20 cm × 10 cm conveyor with laser-cut MDF side plates, four printed rollers on threaded rod, three printed 14 cm arms, and five boxes.
2. **Electronics.** Connect an ESP32 to an A4988 stepper driver, three servos, two 5 V buck converters, and two HX711 load cell amplifiers.
3. **Software.** Write ESP32 firmware and a simple web page that takes a start position and a box, runs the route, and shows which boxes are taken.
4. **Testing.** Measure routing success, route time, box detection, and load limits (section 14).
5. **Plan the full system.** Define how the station will connect to B.L.A.Z.E. and the printers.

---

## 9. Hypothesis

The prototype, a reversible conveyor with three 65° servo arms, can send parts from 10 g to 250 g from any of three start positions to any of five boxes with at least 98% success, in under 8 s per route, using parts that cost under 1% of a robot arm cell.

---

## 10. Methodology

### 10.1 Design History

The first concept (September) was a catch carriage or chutes under the printers that fed lockers opened by a QR code. The team dropped it for a conveyor with servo arms and open boxes. The conveyor has fewer moving parts, needs no locks or scanner, and could be built with parts already in the lab.

| Part | Version | What happened |
| :--- | :--- | :--- |
| Chassis | V1 | Tolerance problems in the structure. |
| Chassis | V2 | Unstable, and the belt did not fit once the bearings and rods were in. |
| Chassis | **V3 (current)** | 65 cm × 20 cm × 10 cm, 5.5 mm MDF. One 10 cm belt, with the boxes in the remaining 10 cm of width to keep the prototype compact. |
| Rollers | V1 | Cone-shaped ends to center the belt. The belt climbed the cones and drifted. With four equal rollers the belt did not touch the middle two, and it was over-tensioned, so the motor struggled. |
| Rollers | **V5 (current)** | Flat flanges fixed the drift. End rollers are 30 mm and middle rollers are 35 mm, which fixed the contact and the tension. |
| Arm | First version | Bad tolerances, used a lot of filament, and too heavy for the servo. |
| Arm | 3 | Failed. At 13 cm it was too short to cover the belt, and the connection to the servo had bad tolerances. |
| Arm | V6 (current, 2 arms) | 14 cm long, with a corrected servo connection. |
| Arm | **V7 (current, 1 arm)** | Same as V6, with a slightly looser fit for the servo. |
| Box | **V1 (all five, current)** | Sized to fit the spaces in the chassis: boxes 1 and 5 are 11 × 11 × 9 cm, and boxes 2, 3, and 4 are 9.5 × 15.5 cm (length × width). Boxes 2 and 4 are shorter because they sit on load cells. Testing showed they are too small for the largest parts (14.3). |

### 10.2 Prototype Layout

![Prototype with dimensions](figures/prototype_dimensions.png)

*Figure 1. The prototype: 65 cm long, 20 cm wide, 10 cm tall.*

![Demo layout](figures/demo_layout.png)

*Figure 2. Demo layout. Gray blocks show where the P1S printers would sit.*

### 10.3 Prototype Sequence

1. **Choose.** The user picks a start position and a box on the web page.
2. **Set the arm.** For box 2, 3, or 4, the ESP32 swings that box's arm out so it meets the part at 65°: the servo goes to 65° or 115°, depending on which way the belt will run. For box 1 or 5, all arms stay in.
3. **Move.** The ESP32 enables the stepper driver and runs the belt toward the chosen box. The part rides the belt until it meets the arm and slides off into the box, or rides off the end. When the start position is right in front of the chosen box (start 1 to box 2, start 2 to box 3, start 3 to box 4), the belt runs toward box 1, so the arm can push the part without first swinging through it.
4. **Stop.** The belt stops when the load cell shows the part has landed (boxes 2 and 4) or after a set distance (boxes 1, 3, and 5). Box 3 has no load cell and runs twice its planned distance, so a part that catches on the arm still reaches the box. If a route takes more than 15 s, the belt stops and the page shows an error.
5. **Log.** The arm swings back, the motor is switched off, and the run is added to the log on the web page: start, box, result, time from Run to arrival, weight before and after, and belt travel.
6. **Status.** The page shows boxes 2 and 4 as taken until the weight returns to near zero.

### 10.4 Full System Sequence (planned)

1. The print finishes. The backend saves a photo from the printer's built-in camera while the part is still on the bed.
2. B.L.A.Z.E. opens the door, pushes the part out, and confirms with its sensor that the bed is clear.
3. The backend picks a free box and sends the route to the ESP32.
4. The station delivers the part and checks its weight against the slicer estimate.
5. The student gets a message with the box number and the photo.
6. When the weight returns to zero, the box is free again.

Two cases the full system has to handle:

* **Two prints finish at the same time.** Only one part is on the belt at a time. Jobs go into a queue, and the second printer keeps its part on the bed until the belt is free.
* **The part does not arrive or the weight is wrong.** The belt stops, the box is marked "needs checking" and is not given to another job, and lab staff get an alert. The system does not retry on its own.

**What B.L.A.Z.E. already provides, and what it does not.** This comes from reading their code and pitch (B.L.A.Z.E. team, 2026a, 2026b).

| Kinetic Kitchen needs | In B.L.A.Z.E. today | Plan |
| :--- | :--- | :--- |
| Which printer finished and whose job it was | Yes. The job record has the user, the printer, and the finish time. | Read it from their system. |
| A signal that the part has left the bed | Not yet. Their code treats "print finished" as "printer free" until the bed sensor is added. | Use their infrared bed-clear sensor once it is installed. How Kinetic Kitchen receives this event is not decided yet. |
| A way to tell the student | Yes. In-app notifications for failed and waiting jobs. | Add a "ready in box N" notification to their app. A separate Discord bot is not needed. |
| Expected part weight | No. Jobs store the file, not the weight. | Read the weight from the sliced .3mf file they store. If that fails, the load cell is used only for occupied or vacant. |
| Photo of the finished part | No. | Open. The printer camera is not part of their system. |

---

## 11. Techniques and Tools

### 11.1 Mechanical

* **Frame.** Two laser-cut side plates of 5.5 mm MDF, 65 cm × 10 cm, held apart by four M8 threaded steel rods that also work as the roller axles.
* **Rollers.** Four rollers printed in PLA, on eight ball bearings with an 8 mm bore, to fit the rod. The bearings are about 21 mm across, which is consistent with standard 608 bearings (8 × 22 mm). The rollers have flat flanges to keep the belt in place. The two end rollers are 30 mm in diameter and the two middle rollers are 35 mm.
* **Belt.** A ready-made conveyor loop, 120 cm around, 10 cm wide, and 2 mm thick. It came from FrED Factory stock.
* **Arms.** Printed in PLA, 14 cm long, one on each SG90 servo. Two are V6 and one is V7 (10.1).
* **Boxes.** Five boxes printed in PLA (V1). Boxes 1 and 5, at the ends, are 11 × 11 × 9 cm. Boxes 2, 3, and 4, along the side, are 9.5 × 15.5 cm outside and 8.9 × 14.9 cm inside (length × width). Box 3 is 9 cm tall. Boxes 2 and 4 rest on load cells, so they are shorter. Every box has a 3 mm floor, so the inside depth is 8.7 cm for boxes 1, 3, and 5, and 6.7 cm for boxes 2 and 4.

### 11.2 Electronics

* **Power.** A 12 V 3 A adapter, through a barrel jack breakout, feeds the stepper driver directly. Two LM2596 buck converters step the 12 V down to 5 V: one for the three servos and one for the ESP32, so a servo pulling current cannot reset the ESP32. Electrolytic capacitors smooth each supply: 100 µF on the A4988 motor supply, 470 µF on the servo rail, and 100 µF on the 3.3 V rail. The ESP32's USB port is only used to flash the firmware.
* **Load cells.** Each 1 kg bar cell connects to its own HX711 board. The data and clock lines go to the ESP32: box 2 to GPIO 26 and 27, box 4 to GPIO 13 and 14.
* **Servos.** The signal wires go to GPIO 23 (box 2), GPIO 19 (box 3), and GPIO 18 (box 4).
* **Motor driver.** A4988 in step and direction mode, full steps, Vref 1.68 V (about 1.5 A per coil with the board's 0.1 Ω sense resistors).
* **Controller.** ESP32 NodeMCU-32S development board.
* **Safety.** A red rocker switch sits between the 12 V barrel jack and the power rail. Switching it off cuts power to everything at once: the stepper driver, both buck converters, the servos, and the ESP32. The belt stops right away, even if the firmware does not respond. The firmware also stops any route that takes more than 15 s.

![Circuit diagram](figures/circuit_diagram.png)

*Figure 3. Circuit diagram: 12 V barrel jack, two LM2596 buck converters, A4988 driver and NEMA 17 motor, ESP32 NodeMCU-32S, two HX711 boards with their load cells, the three SG90 servos, and the emergency switch between the barrel jack and the power rail.*

### 11.3 Software

* **Firmware.** C on the ESP32, built with ESP-IDF v6.1. It steps the motor, moves the servos, reads the load cells, runs the routes, keeps a run log, and serves the web page. The ESP32 joins the lab's Wi-Fi, or opens its own hotspot if that network is not found.
* **Web page.** The page is stored inside the firmware and served by the ESP32, so there is no separate server. It has:
  * start position and box buttons, Run and Stop;
  * a live drawing of the conveyor with arrows that show the belt direction and the arms at their real angle;
  * the status and weight of every box;
  * the run log, with a CSV download;
  * a bring-up panel to jog the belt, run the motor continuously, set each arm's 0 point and out angle, test a servo, and tare and calibrate the load cells.

  The layout adjusts to an ultrawide monitor, a laptop, and a phone.
* **Code location.** Firmware, web page, and test data are in one public repository (Kinetic Kitchen team, 2026a): https://github.com/ceciltheengineer/kinetic-kitchen. The firmware is in `firmware/Kinetic_Kitchen`, and its README lists the pin map, the HTTP API, and the values still to be measured. The test data and analysis are in `experiments/`.
* **Current state.** The electronics are wired and the prototype was tested on November 18 and 19 (section 14).
* **Test data and analysis.** Run data was logged in a Google Sheet ("Parts_Tested") with one tab per route, a parts list with lab scale weights, and a summary tab that calculates success rates, confidence bounds and load cell error. Statistical tests were run in Minitab. A copy of the data (`routing_runs.csv`, `parts.csv`) and the analysis is in the repository's `experiments/` folder.

---

## 12. Infrastructure

The work is done in the FrED Factory Lab and the campus makerspaces:

* Bambu Lab P1S printers (used to print the rollers and arms).
* Laser cutter for the MDF.
* Electronics benches: scales, multimeters, a USB power meter, and soldering stations.

---

## 13. Bill of Materials / Budget

The prototype was built as cheaply as possible. Most parts came from FrED Factory stock, and the team only bought four items.

| # | Item | Used | List price (MXN) | Source |
| :---: | :--- | :--- | ---: | :--- |
| 1 | MDF panel, 5.5 mm, 1.22 × 2.44 m | 20 cm × 65 cm | 274 | Bought |
| 2 | SG90 servo, 5-pack | 3 | 181 | FrED Factory |
| 3 | 12 V 3 A power adapter | 1 | 128 | FrED Factory |
| 4 | ESP32 NodeMCU development board | 1 | 150 | FrED Factory |
| 5 | 1 kg load cell with HX711, 2-pack | 2 | 107 | FrED Factory |
| 6 | A4988 stepper driver | 1 | 30 | FrED Factory |
| 7 | NEMA 17 stepper, StepperOnline 17HS19-2004S1 | 1 | 256 | FrED Factory |
| 8 | GT2 pulley kit (20T and 36T) with 200 mm × 6 mm belt | 1 | 86 | Bought |
| 9 | Washers | 8 | 30 | FrED Factory |
| 10 | Threaded steel rod, M8 | 4 | 119 (estimate) | FrED Factory |
| 11 | Ball bearings, 8 mm bore | 8 | 136 (estimate) | FrED Factory |
| 12 | Conveyor belt loop, 120 cm × 10 cm × 2 mm | 1 | 800 (estimate) | FrED Factory |
| 13 | PLA filament, 1 kg roll | about 1/3 roll | 288 | FrED Factory |
| 14 | LM2596 buck converter, 2-pack | 2 | 125 | FrED Factory |
| 15 | Hot glue gun sticks | 1 | 50 (estimate) | Bought |
| 16 | Barrel DC jack breakout, 5-pack | 1 | 165 | Bought |
| 17 | Red round rocker switch, 2-pin, 12 V | 1 | 10 | FrED Factory |
| | **Total at list price** | | **2,935** | |
| | **Paid by the team** | | **575** | Items 1, 8, 15, 16 |

At 20 MXN per USD, the list price total is about $147 USD. The belt came from lab stock and could not be found for sale online, so its $800 MXN is an estimate. Prices for items 10 and 11 come from listings of similar parts, not from receipts. The price of the glue sticks was not recorded, so $50 MXN is an estimate. List prices are for full packs, so the cost of the material actually used is lower.

The full station has not been costed yet. The first purchase list, for a smaller 1.3 m MDF design (September 30), added up to $4,128 MXN, about $206 USD. The full station in section 15.2 is longer, has a metal frame, a wider belt, and more boxes, so it will cost more. Costing it is step C1 of the plan in section 15.1.

---

## 14. Results

Testing ran on November 17 to 19, 2026. The full experimental design, procedure, statistical tests and figures are in the companion paper *Kinetic Kitchen: Experimental Design and Analysis* (Kinetic Kitchen team, 2026b). All raw data is in the "Parts_Tested" Google Sheet, and a copy is in the repository's `experiments/` folder (11.3).

### 14.1 Tests Run

1. **Routing test.** 24 printed parts from 5.68 g to 48.81 g, in four shapes (triangular, circular, cubical, irregular) and three heights. Every part was run on each belt-only route (boxes 1 and 5). For each arm route (boxes 2, 3, 4), about 10 parts were picked at random, with every shape included. Arm routes ended with 9 to 22 runs each. 252 runs in total. The arms were at 65° (7.1).
2. **Test sample selection and weight limitations.** The weight range of the routing test was constrained by the dimensions of the prototype's collection boxes. Larger printed parts were difficult to accommodate within the existing box openings, limiting the selection of suitable test items. Consequently, the 24 parts used in the routing experiments ranged from 5.68 g to 48.81 g. This does not represent the maximum mechanical capacity of the conveyor or its sorting arms. The 250 g design target was evaluated separately through mechanical load testing, rather than full routing experiments. Routing reliability with heavier parts remains to be validated following improvements to the collection boxes.
3. **Route time.** Every run was timed with a stopwatch, from pressing Run until the part landed.
4. **Box detection.** Occupied status on the web page was recorded for runs into boxes 2 and 4.
5. **Weight accuracy.** Every part was weighed on a lab scale before testing. Each load cell reading in boxes 2 and 4 was compared with that weight.
6. **Belt speed.** Single laps of the 1200 mm belt loop were timed.
7. **Conveyor load.** A 1 kg load was placed on the belt and run the full length.
8. **Arm load.** Each arm pushed heavier loads until it failed. Arm 1 is the arm at box 2, arm 2 at box 3, and arm 3 at box 4.
9. **Not run:** 30-minute drift and drop test.

### 14.2 Results Table

| Measurement | Target | Result | Met? |
| :--- | :--- | :--- | :--- |
| Parts delivered to the chosen box | ≥ 98% | 239 of 252 (94.8%), one-sided 95% lower bound 91.9% | No. 3.2 points below the target. |
| Route time, slowest run | ≤ 8 s | 7.63 s (average 4.0 s) | Yes |
| Box shown as taken after a part lands | Every run | Correct on all 52 runs where status was recorded (the one "not occupied" was a part stuck on the arm that never reached the box) | Yes |
| Box shown as free after pickup | 40 of 40 | Not recorded separately | Not tested |
| Weight error against a lab scale | ≤ 5 g | Margin ±2.75 g, largest error 3.26 g (68 readings) | Yes |
| Belt speed | 150 mm/s | 96.4 mm/s (12.45 s per lap). Was 131.3 mm/s before slowing. | Lowered on purpose to reduce belt slip |
| Belt drift after 30 minutes | None visible | Not tested | |
| Heaviest part the arm can push | ≥ 250 g | Arms: 250 g (fail at 260 g) | Yes |
| Heaviest load the belt can carry | Not set | Carried 1 kg the full length (only load tried) | Limit not reached |

| Group | Runs | Success rate | 95% lower bound |
| :--- | :--- | :--- | :--- |
| Arm routes (boxes 2, 3, 4) | 108 | 92.6% | 87.0% |
| Belt-only routes (boxes 1, 5) | 144 | 96.5% | 92.8% |

### 14.3 Failure Analysis

There were 13 failures in 252 runs:

| Cause | Failures | Share |
| :--- | :--- | :--- |
| Part reached the right box but hung out of it | 10 | 77% |
| Part stuck on the arm (part 12, an 11 g cube, twice) | 2 | 15% |
| Timeout (part 8) | 1 | 8% |

Part 3, a 48.81 g bar whose footprint nearly matches the box opening, caused 5 of the 13 failures in 11 runs. Parts 2 and 5 caused 2 each. Only 3 of 252 runs were routing problems on the belt or arm.

The success rate was recomputed with the box-size failures removed to see what the design can reach:

| Scenario | Successes / runs | Success rate | 95% lower bound |
| :--- | :--- | :--- | :--- |
| As tested | 239 / 252 | 94.8% | 91.9% |
| Part 3 removed | 233 / 241 | 96.7% | 94.1% |
| Boxes large enough for every part | 249 / 252 | 98.8% | 97.0% |

With larger boxes the estimated success rate is above 98%, but the data does not prove it: the lower bound is 97.0%. Showing at least 98% with 95% confidence takes 149 runs with no failures, or 386 runs if the same 3 routing failures happen again.

### 14.4 Outliers

* **Route time.** Grubbs' test on each run's time minus its route average flagged one outlier (+2.44 s, p < 0.001). Part 6, a tall irregular part, accounts for the four slowest runs because it tips and slides slowly along the arm. Without it the slowest run is 6.73 s. With or without it, every run is under 8 s.
* **Load cell.** The two largest errors (−10.2 g and −8.81 g) were parts hanging out of box 2, so the cell only carried part of the weight. They are failed deliveries, not sensor error, and are left out of the accuracy result. Among successful runs Grubbs' test found no outlier (p = 0.41).
* **Calibration.** Box 2 reads about 8% high on heavier parts (gain 1.081). Box 4 is within 2% (gain 1.019). The box 2 calibration factor should be redone.

### 14.5 Entrepreneurship Impact

1. **Problem and opportunity.** Labs that automate bed clearing still sort parts by hand. Parts get damaged and mixed, and time goes to finding prints. Kinetic Kitchen sorts each part into its own box and shows which boxes are taken.
2. **Short-term users.** First, the students and staff who use the FrED Factory printers, together with the B.L.A.Z.E. team. Next, the university makerspace if the FrED Factory trial goes well. After that, other university labs and small print farms that run several Bambu Lab printers.
3. **Can it be sold.** Yes, as a kit. The station uses laser-cut sheet, printed parts, and common electronics. The prototype's parts cost about $2,900 MXN at list price, and an earlier estimate for a smaller full-size design in MDF was about $4,100 MXN. A kit price of about $9,900 MXN (about $495 USD) would cover parts, cutting, printing, and assembly time, and leave a margin of roughly half the price. This price is a first estimate and will be updated once the metal version is costed (section 15.1, step C1).
4. **Difference from existing options.**

| Option | What it does | Cost | Sorts parts by user |
| :--- | :--- | :--- | :--- |
| 3DQue AutoFarm3D with door opener | Opens the printer door and pushes the part off the bed | $129 USD per printer for the door opener, software from $9.99 USD per month (Tom's Hardware, 2025) | No |
| FarmLoop with SimplyPrint | Push-off bed clearing for Bambu Lab printers | Free during beta, paid later (SimplyPrint, n.d.) | No |
| Robot arm cell | Picks the part and places it anywhere | $40,000 to $150,000 USD (Standard Bots, 2024) | Yes |
| Kinetic Kitchen | Routes each part to its own box and tracks the box | About $147 USD in parts for the prototype. The full station is not costed yet (15.1, step C1) | Yes |

5. The ejection products are not direct competitors. They get the part off the bed, and Kinetic Kitchen handles what happens next. The only current product that sorts by user is a robot arm, at more than 250 times the cost.

**Known limits:**

* The prototype belt is too narrow to catch a part pushed out of a printer, so test parts are placed by hand.
* The current boxes are too small for the largest test parts (14.3).
* Small round parts can roll off the open edge of the belt. Side guides are needed.
* Routing tests were limited to parts weighing 5.68–48.81 g because of prototype box dimensions and the availability of suitable test pieces. Full routing performance at the 250 g design limit has not yet been experimentally validated.

---

## 15. Conclusions

1. **The routing works.** Only 3 of 252 parts went wrong on the belt or arm. The reversible belt reaches five boxes with three servos, as designed.
2. **The 98% target was not met as tested.** The prototype delivered 94.8% of parts. That is 3.2 points below the target.
3. **Box size is the main problem.** 10 of the 13 failures were parts that reached the right box and did not fit in it. With large enough boxes the estimated rate is 98.8%, but more runs are needed to confirm it.
4. **Load limits.** The belt carried 1 kg with no trouble. All arms handle 260 g, 250 g target is met by all three arms. A tip stop (A2) should fix arm 1.
5. **Route time meets the target.** Every run finished under 8 s, even with the belt slowed on purpose to 96.4 mm/s (from the 150 mm/s design speed) to reduce slip.
6. **The load cells meet the 5 g target** with a ±2.75 g margin, and box status was correct on every recorded run. Box 2 needs its calibration factor redone.
7. **The mechanical fixes held.** Flat roller flanges and 5 mm larger middle rollers kept the belt tracking and tensioned through all 252 runs.

### 15.1 Improvement Plan

Each improvement below comes from a weakness found in this report. They are ordered with the changes that make the test results trustworthy, then the work that has to be settled before the full station is built.

**Phase A: Easy Fixes**

| # | Upgrade | Why | How | Done when |
| :--- | :--- | :--- | :--- | :--- |
| A1 | Side guides on the belt | Round parts roll off the open edge (14.5). A part that falls off counts as a failed route. | Printed or MDF strips along the open edge, with gaps only at boxes 2, 3, and 4. | No part leaves the belt except into a box. |
| A2 | Stop at the arm tip | The SG90 is at its limit with a 250 g part (7.1). Arm 1 failed at 250 g in the load test, and part 12 got stuck on the arm twice (14.3). | A fixed post where the arm tip rests at 65°, so the belt's push goes into the frame and not the servo. | The arm holds a 250 g part without moving back. |
| A3 | Larger box openings or an angled entry | 10 of 13 failures were parts hanging out of the box (14.3). | Widen the box opening or add a ramp so parts like part 3 drop fully inside. | Part 3 lands fully inside every box in 10 runs each. |
| A4 | Recalibrate the box 2 load cell | Box 2 reads about 8% high (14.4). | Redo the HX711 scale factor with a known mass. | Gain within 2% of 1.000. |
| A5 | Belt speed and slip | The step rate was lowered to stop the belt slipping, so the belt runs at 96.4 mm/s, not 150 mm/s (7.2). | Raise belt tension or add grip to the drive roller, then raise the step rate again. | Belt reaches 150 mm/s with no visible slip over 30 minutes. |

**Phase B: next semester (February to June 2027). Connect to B.L.A.Z.E. and settle the geometry, still on the prototype.**

| # | Upgrade | Why | How | Done when |
| :--- | :--- | :--- | :--- | :--- |
| B1 | Connection to B.L.A.Z.E. | The station should start a route when a part leaves the bed, not when someone clicks a button. Their bed-clear signal is not built yet, so both teams have to agree on it early (10.4). | Take the printer, job, and user from the B.L.A.Z.E. job record and start the route on its bed-clear signal. Queue the jobs and allow one part on the belt at a time. | A real print ends up in a prototype box with no human input. |
| B2 | Student message | The student needs to know which box to go to. | Add a "ready in box N" notification to the B.L.A.Z.E. app. Add the printer camera photo if a way to pull it is found. | The student gets the message within 30 s of delivery. |
| B3 | Catch test with a real printer | The prototype has only been tested with parts placed by hand. How a part leaves a B.L.A.Z.E. printer sets the belt width, belt height, and side guides. | Put the prototype in front of one printer with B.L.A.Z.E. installed and record where and how parts land. | Landing zone, drop height, and bounce are measured for at least 30 parts. |
| B4 | Box geometry | The current box is too small for the largest test parts (14.3). The box has to fit the largest part, sit on a load cell, and let the part slide in without bouncing out. | Redesign and test the box, its ramp, and its padding with parts of different sizes. | A box design that accepts the largest part a P1S can print. |
| B5 | Arm and servo | The arm is the weak point (7.5). Arm 3.0 failed on length and on its servo connection. | Test a longer arm with a tip stop and an MG996R on the prototype. | The arm holds a 500 g part without moving back. |
| B6 | Load cells on all five boxes, with weight check | Box status only works for boxes with a load cell, and the weight check confirms the right part arrived (7.3). | Add three more load cells. Read the expected weight from the sliced file that B.L.A.Z.E. stores. | Every box shows occupied or vacant, and a wrong-weight part is flagged. |
| B7 | Confirm the 98% target | The current data estimates 98.8% with larger boxes but cannot prove it (14.3). | Run at least 149 runs with the new boxes, with parts up to 250 g. | Success rate lower bound of at least 98%. |
| B8 | Final dimensions | The full station should not be built on guesses. | Use the results of B3 to B5 to fix the length, belt width, and box size in 15.2. | A dimensioned drawing of the full station. |

**Phase C: full station (2027 to 2028)**

| # | Upgrade | Why | Done when |
| :--- | :--- | :--- | :--- |
| C1 | Cost the full station | The entrepreneurship case needs a real price (14.5). | A full BOM and a kit price that covers parts, labor, and support. |
| C2 | Build the full station (15.2) | This is the actual product. | All 12 boxes can be reached from all 3 printers. |
| C3 | Pilot in FrED Factory | A lab test does not prove the station works with real prints from real students. | Two weeks on all lab prints, with at least 98% of jobs delivered to the right box. |
| C4 | Locked boxes with student ID | Open boxes record when a part is removed, but not who removed it. | A box opens only for the student who owns the part. |

### 15.2 Full Station Plan

This is the proposed design for the full station. The numbers are a starting point. Phase B of the plan above will confirm or change them.

| Item | Proposed | Reason |
| :--- | :--- | :--- |
| Printers | 3 Bambu Lab P1S in a row, with B.L.A.Z.E. | The lab's current setup. |
| Position | In front of the printers, below the doors | B.L.A.Z.E. pushes parts out through the front door. |
| Belt width | 30 cm | The P1S build plate is 256 mm wide, so a narrower belt cannot hold the largest parts. |
| Length | About 2.8 m | The printers only need 1.25 m. The boxes need more. The printers block one side of the belt, so all side boxes go on the other side: 10 boxes at about 28 cm each. |
| Boxes | 12: 10 along the side and 1 at each end | Enough for one day of prints. A single row keeps every box at the same height and easy to reach. |
| Box assignment | Nearest vacant box to the printer | Keeps most routes short (about 6 s). The longest is about 17 s (7.5). |
| Arms | 10 arms of about 33 cm at 65°, MG996R servos, with a tip stop | A 30 cm belt needs a longer arm, and the servo alone has almost no margin with a 500 g part at that length (7.5). |
| Servo control | 16-channel servo board (PCA9685) | Ten servos need more outputs than the ESP32 has free once the load cells are connected. |
| Weight sensing | Load cell under every box, 1 kg or larger | Needed for box status and for the weight check. A 1 kg cell covers the 500 g design limit. |
| Frame | Metal (aluminum extrusion) | MDF sags and swells over this length, and the station has to run every day. |
| Motor | NEMA 17 with a 3:1 reduction and 50 mm rollers | Margin of about 2.7 by calculation (7.5). A larger motor is the fallback. |
| Safety | Stop button, fuse, side guides, covered pulleys | Students will be near it every day. |

The other option is to keep the conveyor short, about 1.3 m, and stack the boxes in two levels. That saves floor space but needs a way to lift or drop parts between levels, which adds a mechanism. The single long row was chosen because it reuses what the prototype already proves.

---

## 16. Resources

### 16.1 Quick Links

* [Fabricated Parts](https://docs.google.com/spreadsheets/d/1gsep8SdvaHTiXaHkLjknKFcyxpkXOT27pumggKACxK0/edit?gid=0#gid=0)
* [Circuit Diagram](https://app.cirkitdesigner.com/project/a55b3d68-53eb-42b1-83a7-80fec2e8fb32)
* [GitHub](https://github.com/ceciltheengineer/kinetic-kitchen)
* [Experimentation Data](https://docs.google.com/spreadsheets/d/1HfluxEijzR6Z8CE1EViMvRoUbhrTB4ri4SRMgblZ7ek/edit?gid=777000001#gid=777000001)
* [Minitab Report](https://docs.google.com/document/d/16e-PjFZlRrLRySKyIGI4oB9TyIkQwSREruUrsrmPsow/edit?tab=t.0)
* [Experimental Design and Analysis](https://docs.google.com/document/d/1gMVyrIyX6BP9FqIETf1XgWATHuKGHCi564KJu5fjsUU/edit?tab=t.0)

### 16.2 References

1. Allegro MicroSystems. (n.d.). *A4988 DMOS microstepping driver with translator and overcurrent protection* [Datasheet]. https://www.allegromicro.com/-/media/files/datasheets/a4988-datasheet.pdf
2. Avia Semiconductor. (n.d.). *HX711 24-bit analog-to-digital converter (ADC) for weigh scales* [Datasheet]. https://www.lcsc.com/datasheet/lcsc_datasheet_Avia-Semicon-Xiamen-HX711_C43656.pdf
3. Bambu Lab. (n.d.). *Bambu Lab P1S*. Retrieved November 10, 2026, from https://bambulab.com/en/p1
4. B.L.A.Z.E. team. (2026a). *B.L.A.Z.E.* [Source code]. GitHub. https://github.com/Isai-JQ/B.L.A.Z.E
5. B.L.A.Z.E. team. (2026b). *B.L.A.Z.E.: Bambu Lab Automated Zero-Touch Executor* [Pitch slides, internal document]. FrED Factory Lab, Tecnológico de Monterrey.
6. DiCristofaro, A. S. (2026, July 7). *MIT-designed educational factory embraces modern manufacturing*. MIT Department of Mechanical Engineering. https://meche.mit.edu/node/4490
7. Fab Foundation. (n.d.). *Global community*. Retrieved November 9, 2026, from https://fabfoundation.org/global-community
8. FrED Factory Lab. (n.d.). *Research areas of the FrED Factory Lab* [One-page summaries, internal document]. Tecnológico de Monterrey.
9. *How to make a DIY belt conveyor controlled by Arduino* [Video]. (2022, June 5). YouTube. https://www.youtube.com/watch?v=5sep7IT5YgA
10. Kinetic Kitchen team. (2026a). *Kinetic Kitchen* [Source code and test data]. GitHub. https://github.com/ceciltheengineer/kinetic-kitchen
11. Kinetic Kitchen team. (2026b). *Kinetic Kitchen: Experimental design and analysis* [Companion paper, internal document]. FrED Factory Lab, Tecnológico de Monterrey.
12. MIT FrED Factory. (n.d.). *FrED Factory*. Retrieved November 10, 2026, from https://fredfactory.mit.edu/
13. SimplyPrint. (n.d.). *The FarmLoop clearing method for AutoPrint*. Retrieved November 9, 2026, from https://help.simplyprint.io/en/article/the-farmloop-clearing-method-for-autoprint-1ttb53x/
14. Standard Bots. (2024, October 7). *Universal Robots price guide: What to expect (new and used costs)*. https://standardbots.com/articles/universal-robot-price
15. StepperOnline. (n.d.). *17HS19-2004S1 NEMA 17 bipolar stepper motor* [Datasheet]. https://www.omc-stepperonline.com/download/17HS19-2004S1.pdf
16. Tom's Hardware. (2025, August 4). *New auto-ejection tool for Bambu Lab print farms automatically ejects finished 3D prints from the machine*. https://tomshardware.com/3d-printing/new-auto-ejection-tool-for-bambu-lab-print-farms-automatically-ejects-finished-3d-prints-from-the-machine-usd129-kit-includes-auto-door-opener-and-special-bed-surface-for-frictionless-part-ejection
17. Groover, M. P. (2019). *Automation, production systems, and computer-integrated manufacturing* (5th ed.). Pearson.
18. Kay, M. G. (2012). *Material handling equipment*. Fitts Department of Industrial and Systems Engineering, North Carolina State University. https://mgkay.github.io/Material_Handling_Equipment.pdf
