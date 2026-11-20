# Kinetic Kitchen: Experiment Files

Everything from the Nov 18 to 19, 2026 prototype testing, for anyone building on the results.

| File | What it is |
| :--- | :--- |
| Experimental_Design_and_Analysis.md | Full experiment paper: design, procedure, analysis, results, conclusions |
| routing_runs.csv | One row per routing run (252 runs): start, box, part, success, time, load cell reading, error, occupied, failure cause |
| parts.csv | The 24 test parts: shape, height, color, lab scale weight |
| other_tests.md | Conveyor and arm load tests, load cell accuracy, belt speed, load cell limit |
| minitab_results.md | Minitab test outputs |
| figures/ | Charts (fig_*) and Minitab/Sheets screenshots (ss_*) |

Live sources: Google Sheet "Parts_Tested" (raw data and formulas), Google Doc "Kinetic Kitchen - Experimental Design and Analysis", Minitab project "Kinetic Kitchen Prototype".

## Headline numbers
- 239 of 252 parts delivered (94.8%, 95% lower bound 91.9%). Target 98% not met; within the school's ±5%.
- 10 of 13 failures: part reached the right box but did not fit. Bigger boxes would give an estimated 98.8%.
- Slowest route 7.63 s (target ≤ 8 s). Belt 96.4 mm/s (slowed on purpose to reduce slip).
- Load cells ±2.75 g (target ≤ 5 g). Box 2 needs recalibration (gain 1.081).
- Conveyor carried 1 kg. Arms: 250 g on arms 2 and 3, arm 1 fails at 250 g.
