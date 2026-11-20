# Experimental Design and Analysis: Kinetic Kitchen Sorting Prototype

Expo Ingenierías 2026, FrED Factory Lab. Advisor: Dr. Erick Ramírez-Cedillo.
Team: Cecil Yusei Howell, Carlos Alberto Garcia Leyva, Yoana Montoya Garza, Javier Taddeus Hernández Garza.

> Markdown copy of the Google Doc "Kinetic Kitchen - Experimental Design and Analysis" (Nov 19, 2026). Raw data: `routing_runs.csv`, `parts.csv`. Other tests: `other_tests.md`. Minitab output: `minitab_results.md`. Figures: `figures/`.

## 1. Purpose

Kinetic Kitchen is a conveyor that sends a finished 3D print to an assigned box. This paper describes how we tested the 65 cm prototype in November 2026, how the data was collected and analyzed, and what the results say about the design. The tests answer four questions: how often a part reaches the chosen box, how long a route takes, how accurate the box load cells are, and what has to change before the full station is built.

## 2. Hypothesis and Acceptance Criteria

The targets below were set in the project report before testing. They were not changed after the data came in.

| Measure | Target set before testing | How it is judged |
| :--- | :--- | :--- |
| Parts delivered to the chosen box | At least 98% | Observed success rate, with a 95% confidence bound |
| Route time, slowest run | 8 s or less | Longest recorded route time |
| Load cell error against a lab scale | 5 g or less | Margin of error: \|average error\| + 2 standard deviations |
| Box shown as occupied after a part lands | Every run | Count of correct status readings |

Our school accepts results within 5% of a stated target. Each result is reported against the original target first, then against that 5% band.

## 3. Experimental Design

### 3.1 Factors and levels

| Factor | Levels | Notes |
| :--- | :--- | :--- |
| Start position | 3 (positions 1, 2, 3) | Stand in for the three Bambu Lab P1S printers |
| Destination box | 5 (boxes 1 to 5) | Boxes 1 and 5 are reached by the belt alone. Boxes 2, 3, 4 need a 45° servo arm. |
| Part | 24 printed parts | Four shapes (triangular, circular, cubical, irregular), three heights, 5.68 g to 48.81 g |

Every combination of start position and box is a route, which gives 15 routes. This is a full factorial design on route (3 × 5), with part as a blocking factor that adds shape and weight variety.

### 3.2 Response variables

- Success (yes or no): the part ends fully inside the chosen box before the 10 s firmware timeout.
- Route time (s): from the start command to the part landing in the box.
- Load cell reading (g): boxes 2 and 4 only, compared with the part weight from a lab scale.
- Occupied status: whether the web page showed box 2 or 4 as taken after the run.

### 3.3 Sample size and selection

Routes to boxes 1 and 5 are fast to run, so all 24 parts were run on each of those 6 routes (144 runs). For the 9 routes that use an arm (boxes 2, 3, 4), 10 parts per route were picked at random from the 24, with the rule that each sample had to include all four shapes. Some arm routes ended with more than 10 runs. The total is 252 runs.

With zero failures, 149 runs are enough to show a rate of at least 98% with 95% confidence. We aimed for more than 149 runs in case of failures.

### 3.4 Controls

- Same prototype, same firmware and the same belt speed setting for every run. No mechanical changes once counted runs started.
- Each part was placed by hand at a taped start mark.
- Every part was weighed on a lab scale before testing and given an ID.
- A failed run was logged with its cause, not repeated or removed.

## 4. Equipment and Materials

- Prototype: 65 cm conveyor, NEMA 17 stepper with A4988 driver (full steps, Vref 1.68 V), ESP32, three SG90 servo arms at 45°, five boxes.
- Two 1 kg bar load cells with HX711 amplifiers under boxes 2 and 4.
- Lab scale, stopwatch and the firmware time log.
- 24 printed test parts (`parts.csv`).
- Google Sheets workbook "Parts_Tested" for data entry and calculation.
- Minitab (web) for statistical tests and charts.

## 5. Procedure

### 5.1 Setup

1. Weigh every test part on the lab scale and record weight, shape, height and color.
2. Generate the random part list for each arm route.
3. Tare the load cells in boxes 2 and 4 with the boxes empty.
4. Run one pilot part per route to check the setup. Pilot runs are not counted.

### 5.2 Routing runs (one run)

1. Select the start position and box on the web page.
2. Place the part at the taped mark for that start position.
3. Press Run and start timing.
4. Stop timing when the part lands in a box or the belt stops on timeout.
5. Record Yes if the part is fully inside the chosen box, otherwise No and the failure cause.
6. For boxes 2 and 4, record the load cell reading and whether the page shows the box as occupied.
7. Remove the part and check the page shows the box as empty again.

### 5.3 Belt speed

A mark on the belt was timed over single laps of the 1200 mm belt loop, before and after the step rate was lowered. The first lap starts from standstill, so it was recorded separately.

### 5.4 Load cell accuracy

Every part already had a lab scale weight, so each successful run into box 2 or 4 gave one reading to compare against it. Runs where the part hung out of the box were left out, because part of the weight rested on the box edge.

### 5.5 Load tests

- Conveyor: a 1 kg load was placed on the belt and run the full length.
- Arms: each arm pushed heavier loads until it failed.

## 6. Data Collection and Google Sheets Integration

| Tab | What it holds |
| :--- | :--- |
| Base Data | Part ID, shape, height, color and lab scale weight for all 24 parts |
| 15 route tabs (Pos 1 -> Box 1 to Pos 3 -> Box 5) | One row per run: success, time, failure cause. Box 2 and 4 tabs add load cell reading, actual weight, error and occupied status. |
| Summary | Runs, successes, success %, 95% lower bound, mean and max time per route and per group, load cell statistics |
| Other Tests | Conveyor and arm load tests, load cell accuracy by box, belt speed, load cell capacity |

Main formulas:

- Actual weight in each box 2 and 4 tab is looked up from Base Data with INDEX/MATCH on part ID. Error = reading − actual.
- Success, occupied and failure cause columns use dropdowns so entries can be counted.
- The Summary tab finds each column by header name (MATCH on "Success?" and "Time (s)"), so moving a column does not break totals.
- 95% lower bound: Clopper-Pearson, `BETA.INV(0.05, k, n − k + 1)`, or `0.05^(1/n)` with no failures.
- Load cell statistics stack the six box 2 and 4 tabs and use FILTER to keep only successful runs.

| Route | Runs | Successes | Success % | 95% lower bound | Mean time (s) | Max time (s) | Mean \|error\| (g) | Max \|error\| (g) | Shown occupied % |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Pos 1 -> Box 1 | 24 | 23 | 95.8% | 81.7% | 1.7 | 1.8 | | | |
| Pos 1 -> Box 2 | 22 | 20 | 90.9% | 74.1% | 3.3 | 5.1 | 1.08 | 3.26 | 100.0% |
| Pos 1 -> Box 3 | 13 | 12 | 92.3% | 68.4% | 5.1 | 7.4 | | | |
| Pos 1 -> Box 4 | 12 | 12 | 100.0% | 77.9% | 6.0 | 6.5 | 0.77 | 1.93 | 100.0% |
| Pos 1 -> Box 5 | 24 | 23 | 95.8% | 81.7% | 5.4 | 5.6 | | | |
| Pos 2 -> Box 1 | 24 | 23 | 95.8% | 81.7% | 3.4 | 3.9 | | | |
| Pos 2 -> Box 2 | 9 | 7 | 77.8% | 45.0% | 5.2 | 7.6 | 1.30 | 2.22 | 75.0% |
| Pos 2 -> Box 3 | 10 | 9 | 90.0% | 60.6% | 3.1 | 4.3 | | | |
| Pos 2 -> Box 4 | 10 | 9 | 90.0% | 60.6% | 5.3 | 7.0 | 0.73 | 3.07 | 100.0% |
| Pos 2 -> Box 5 | 24 | 24 | 100.0% | 88.3% | 3.3 | 3.5 | | | |
| Pos 3 -> Box 1 | 24 | 23 | 95.8% | 81.7% | 5.5 | 5.6 | | | |
| Pos 3 -> Box 2 | 10 | 10 | 100.0% | 74.1% | 6.3 | 6.7 | 1.36 | 3.25 | 100.0% |
| Pos 3 -> Box 3 | 11 | 11 | 100.0% | 76.2% | 5.0 | 5.9 | | | |
| Pos 3 -> Box 4 | 11 | 10 | 90.9% | 63.6% | 3.5 | 5.0 | 0.35 | 1.03 | 100.0% |
| Pos 3 -> Box 5 | 24 | 23 | 95.8% | 81.7% | 1.7 | 1.8 | | | |

For the statistical tests, the run data was moved from the sheet into Minitab with its session command line, so Minitab and the sheet use the same data.

## 7. Analysis Methods

| Question | Method | Tool |
| :--- | :--- | :--- |
| How often does a part reach the box? | One-proportion test against 98% (exact Clopper-Pearson) | Minitab, Google Sheets |
| What causes the failures? | Pareto chart of failure causes | Minitab |
| Are any runs unusually slow? | Grubbs' test on each run's time minus its route average | Minitab |
| Is the load cell accurate? | Linear regression of reading on lab weight, error statistics, Grubbs on the error | Minitab, Google Sheets |
| Can 98% be reached? | Recompute success rate and bound with specific causes removed | Minitab, Python |

Route times were compared after subtracting each route's own average, because routes differ in length.

## 8. Results

### 8.1 Routing success

| Group | Runs | Successes | Success rate | 95% lower bound |
| :--- | ---: | ---: | ---: | ---: |
| All routes | 252 | 239 | 94.8% | 91.9% |
| Arm routes (boxes 2, 3, 4) | 108 | 100 | 92.6% | 87.0% |
| Belt-only routes (boxes 1, 5) | 144 | 139 | 96.5% | 92.8% |

The prototype delivered 239 of 252 parts (94.8%). The one-proportion test rejects 98% (p = 0.002), so the 98% target was not met as tested. The result is 3.2 percentage points below the target, inside the 5% band the school accepts.

Of the 13 failures, 10 were parts that reached the correct box but did not fit fully inside it. Only 3 were routing problems: part 12 got stuck on the arm twice and part 8 timed out once.

| Part | Description | Runs | Failures | Cause |
| :--- | :--- | ---: | ---: | :--- |
| 3 | Irregular, short, 48.81 g bar | 11 | 5 | Hanging out of box (part is almost the size of the box) |
| 2 | Circular, short, 36.75 g | 11 | 2 | Hanging out of box |
| 5 | Cubical, short, 24.70 g | 10 | 2 | Hanging out of box |
| 12 | Cubical, short, 11.00 g | 9 | 2 | Stuck on arm |
| 6 | Irregular, tall, 15.61 g | 11 | 1 | Hanging out of box |
| 8 | Triangular, short, 9.43 g | 11 | 1 | Timeout |

### 8.2 Route time

Every timed run finished under 8 s. The longest was 7.63 s and the average was 4.0 s. Routes where the part rides off the near end of the belt (P1-B1, P3-B5) took about 1.7 s. See `figures/fig_time.png`.

### 8.3 Outliers and their impact

| Data | Outlier | Grubbs result | Cause | Impact |
| :--- | :--- | :--- | :--- | :--- |
| Time minus route average (245 timed runs, before 2 late rows) | +2.44 s (part 6, P2-B2) | G = 6.37, p < 0.001 | Tall part tips and slides slowly along the arm | Part 6 is the four slowest runs. Without it the slowest run is 6.73 s. Neither case breaks 8 s. |
| Load cell error, all runs (73 readings) | −10.2 g and −8.81 g | G = 5.05, p < 0.001 | Parts 5 and 3 hanging out of box 2 | With them the margin is about ±4 g. They are failed deliveries, not sensor error. |
| Load cell error, successful runs (68 readings) | None | G = 2.68, p = 0.41 | n/a | The ±2.75 g margin holds. |

### 8.4 Load cell accuracy

| | Box 2 | Box 4 | Both |
| :--- | ---: | ---: | ---: |
| Readings | 37 | 31 | 68 |
| Average error (g) | +0.78 | −0.36 | +0.26 |
| Standard deviation (g) | 1.29 | 0.85 | 1.24 |
| Largest error (g) | 3.26 | 3.07 | 3.26 |
| Margin of error, \|average\| + 2 SD (g) | ±3.36 | ±2.07 | ±2.75 |
| Gain (1.000 is perfect) | 1.081 | 1.019 | 1.051 |

Both boxes meet the 5 g target. Regression: reading = −0.57 + 1.051 × actual, R² = 98.8%. Box 2 reads about 8% high on heavier parts, which points to a calibration factor error. Box 4 is within 2%.

Every part that landed in box 2 or 4 was shown as occupied. The one "not occupied" run was part 12, which got stuck on the arm and never reached the box, so that reading was also correct.

### 8.5 Belt speed

| Condition | Lap time (s) | Speed (mm/s) | Share of 150 mm/s design |
| :--- | ---: | ---: | ---: |
| Before slowdown, first lap | 9.30 | 129.0 | 86% |
| Before slowdown, steady | 9.14 | 131.3 | 88% |
| After slowdown, first lap | 12.80 | 93.8 | 63% |
| After slowdown, steady (all routing runs) | 12.45 | 96.4 | 64% |

The step rate was lowered on purpose to reduce belt slip, cutting speed by 26.6%. In full-step mode 96.4 mm/s is about 409 steps/s.

### 8.6 Load limits

- Load cells: 1 kg rated. A part over 1 kg needs a 2 kg or 5 kg cell.
- Conveyor: carried 1 kg the full length. Only load tried, so the real limit is higher.
- Arms (assumed arm 1 = box 2, arm 2 = box 3, arm 3 = box 4): arm 1 fails at 250 g; arms 2 and 3 pass 250 g and fail at 260 g. Below the failure load the arm is pushed back a little but the part still goes in. Two of three arms meet the 250 g target.

## 9. Can the 98% Target Be Reached?

| Scenario | Successes / runs | Success rate | 95% lower bound | Below 98%? (p) |
| :--- | :--- | ---: | ---: | :--- |
| As tested | 239 / 252 | 94.8% | 91.9% | Yes (0.002) |
| Part 3 removed | 233 / 241 | 96.7% | 94.1% | No (0.11) |
| Bigger boxes (no hanging-out failures) | 249 / 252 | 98.8% | 97.0% | No |
| Both | 238 / 241 | 98.8% | 96.8% | No |

With large enough boxes the estimated rate is 98.8%, but the lower bound is 97.0%, so 98% is not proven. To show at least 98% with 95% confidence:

| Failures allowed | Runs needed |
| ---: | ---: |
| 0 | 149 |
| 1 | 236 |
| 2 | 313 |
| 3 | 386 |

## 10. Limitations

- Parts were placed by hand, so the test covers routing, not catching a part ejected from a printer.
- Success for boxes 1, 3 and 5 was judged by eye (no load cell).
- Times in box 2 and 4 tabs were logged to 0.01 s, the others to 0.1 s. Pos 2 -> Box 4 parts 7 and 10 were logged as a flat 7 s.
- Runs per route ranged from 8 to 24, so per-route rates are less certain than the overall rate.
- The heaviest routing part was 48.81 g. Only the load tests reached 250 g and above.
- The conveyor load test used one 1 kg trial. Power and 30-minute drift were not measured.

## 11. Conclusions

- The routing works. Only 3 of 252 parts went wrong on the belt or arm.
- The 98% target was not met as tested (94.8%), but the result is within the school's 5% band.
- Box size is the main problem: 10 of 13 failures were parts that reached the right box and did not fit. Part 3 caused half of those.
- Every route finished under 8 s, even with the belt slowed to 96.4 mm/s.
- Load cells meet the 5 g target with a ±2.75 g margin. Box 2 needs recalibration.
- Box status detection was correct on every run.

### Next steps

1. Make the boxes larger or add an angled entry.
2. Recalibrate the box 2 load cell.
3. Add a fixed stop or smoother arm face for small cubes like part 12.
4. Run at least 149 more runs with the new boxes to confirm 98%.
5. Add a tip stop so arm 1 holds 250 g.
