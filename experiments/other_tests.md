# Kinetic Kitchen: Other Tests (Nov 19, 2026)

Source: "Parts_Tested" Google Sheet, Other Tests tab.

## Test 1: Conveyor maximum load
- Method: load in the middle of the belt, run the full length forward and back.
- Result: 1000 g carried the full length (1 trial). Only load tried, so the limit was not reached.

## Test 2: Arm maximum load
- Method: arm at 65°, load placed at the start farthest from the box, raise the load until it fails.
- Arm mapping assumed: arm 1 = box 2, arm 2 = box 3, arm 3 = box 4.

| Arm | Box | Passes | Fails at |
| :--- | :--- | :--- | :--- |
| 1 | 2 | below 250 g | 250 g |
| 2 | 3 | 250 g | 260 g |
| 3 | 4 | 250 g | 260 g |

Below the failure load the arm was pushed back a little, but the part still went into the box. Target was ≥ 250 g: met by 2 of 3 arms.

## Test 3: Load cell margin of error (successful runs only)

| Box | Readings | Bias (g) | SD (g) | Mean \|error\| (g) | Max \|error\| (g) | Margin \|bias\| + 2 SD (g) | Gain |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 2 | 37 | +0.78 | 1.29 | 1.19 | 3.26 | 3.36 | 1.081 |
| 4 | 31 | −0.36 | 0.85 | 0.62 | 3.07 | 2.07 | 1.019 |
| Both | 68 | +0.26 | 1.24 | 0.93 | 3.26 | 2.75 | 1.051 |

Box 2 reads about 8% high on heavier parts: redo its HX711 calibration factor.

## Test 4: Success rate

| Group | Runs | Success % | 95% lower bound |
| :--- | ---: | ---: | ---: |
| Arm push (boxes 2, 3, 4) | 108 | 92.6% | 87.0% |
| Belt only (boxes 1, 5) | 144 | 96.5% | 92.8% |
| All routes | 252 | 94.8% | 91.9% |

## Test 5: Belt speed (1200 mm belt loop)

| Condition | Lap (s) | Speed (mm/s) | % of 150 mm/s |
| :--- | ---: | ---: | ---: |
| Before slowdown, lap 1 | 9.30 | 129.0 | 86% |
| Before slowdown, steady | 9.14 | 131.3 | 88% |
| After slowdown, lap 1 | 12.80 | 93.8 | 63% |
| After slowdown, steady (used in testing) | 12.45 | 96.4 | 64% |

Step rate lowered on purpose to reduce belt slip (−26.6%). Driver: A4988, full steps, Vref 1.68 V, R100 sense resistors (about 1.5 A per coil). 96.4 mm/s ≈ 324 steps/s (belt moves 1.263 times the roller formula distance).

## Test 6: Load cell limit
1 kg bar cells under boxes 2 and 4. Parts over 1 kg need a 2 kg or 5 kg cell.

## Not tested
Power on the 12 V line, 30-minute belt drift, drop test.
