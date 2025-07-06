# 🌟 Eko's TbaMUD Playground

_A modern, ongoing reimagination of TbaMUD – combining classic inspired features with contemporary improvements._

---

## ✨ Features & Enhancements

### 🧩 **Account System**
- Players log in via accounts instead of per-character credentials.
- Accounts can manage multiple characters.
- Initial version complete; planned expansions include password recovery and web integration.

---

### ⚔️ **Combat & Progression**
- **Dynamic EXP Tables**  
  - EXP requirements scale dynamically by class.
  - Easy to tweak experience progression in code.
- **Critical Hit System**  
  - Chance to land critical hits based on gear and buffs.
  - Both **Crit Chance** and **Crit Damage** displayed in score.
  - Works for both physical and magical attacks.
- **EXP Bonus System**  
  - Equipment and buffs grant bonus experience percentages.
  - Clearly shown in the score screen.

---

### 🌍 **Exploration & Tracking**
- **Zone Discovery**  
  - Tracks which zones each player has explored.
  - Displays in score as "Zones Discovered."
- **Combat Record**  
  - Counts total kills, legit kills, and deaths for each player.
  - Kill list to control experience per kill of same mob.
  - All stats displayed in score.

---

### 🛠️ **Helpers & Utility**
- New helper functions:
  - `get_crit_chance()` – Calculates total critical chance.
  - `get_crit_damage()` – Calculates total critical damage.
  - `get_exp_percentage_bonus()` – Calculates experience gain bonuses.
  - AC is now max of 999 (99,9%) and is Physical Resistance.
  - All atributes are now max of 999.
- Prepared infrastructure for future systems:
  - Magical Resistance
  - Elemental Resistance
  - Regeneration mechanics

---

## 🚧 TODO & Planned Features
- Implement magical and elemental resistance calculations in combat.
- Expand the account system (e.g., password recovery, email verification).
- Add regeneration systems for HP/Mana/Move.
- Extend quest and exploration tracking.
- Achievements.
- Random Items based on zone lvl.
- Split up the gameplay in 5 parts based on level, 5 towns.
- Instances.
- Alot more..

---

> **Note:** This project is built on a customized TbaMUD codebase.
> Always refer to [the repository](https://github.com/Ekot84/tbamud-playground) as the primary source of truth.

