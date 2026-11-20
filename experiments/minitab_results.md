# Kinetic Kitchen: Minitab Results (project "Kinetic Kitchen Prototype", Nov 19, 2026)

| Test | Data | Result |
| :--- | :--- | :--- |
| One-proportion (Clopper-Pearson), H0 p = 0.98 vs p < 0.98 | 239 of 252 | p̂ = 0.948, 95% upper bound 0.969, p = 0.002. 98% rejected. |
| Pareto of failure causes | 13 failures | Hanging out of box 10 (77%), stuck on arm 2 (15%), timeout 1 (8%) |
| Grubbs, time minus route average | 245 timed runs | Max +2.44 s (part 6, P2-B2), G = 6.37, p < 0.001 |
| Grubbs, load cell error, all runs | 73 readings | Min −10.2 g, G = 5.05, p < 0.001 (parts hanging out of box 2) |
| Grubbs, load cell error, successful runs | 68 readings | Mean 0.260, SD 1.244, G = 2.68, p = 0.41. No outlier. |
| Regression, reading vs actual (successful runs) | 68 readings | Reading = −0.5721 + 1.051 × Actual, S = 1.147, R² = 98.80% |

Minitab worksheet columns ending in `_v2` (C40 to C44) hold the corrected load cell data.
