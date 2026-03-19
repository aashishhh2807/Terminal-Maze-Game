# 🧩 LABYRINTH — Terminal Maze Game

LABYRINTH is a fully interactive terminal-based maze game written in C.
Navigate through different maze levels, collect rewards, avoid ghosts, and escape to the exit!

---

## 🎮 Features

* 🟢 **3 Difficulty Levels**

  * Easy — Small maze, no enemies
  * Medium — Larger maze with more rewards
  * Hard — Complex maze with moving ghosts 👻

* 🎯 **Gameplay Mechanics**

  * Collect coins (`.`) → +10 points
  * Collect powerups (`+`) → +1 life
  * Avoid ghosts (`G`) → -2 lives
  * Hit walls (`#`) → -1 life

* ❤️ **Health System**

  * Start with 5 lives
  * Bonus score based on remaining lives

* 🏆 **Score System**

  * Exit bonus: +500 points
  * Extra: +50 per remaining life

* 💾 **Persistent Scoreboard**

  * Stores player stats locally
  * Tracks total score, games played, and wins

* 🎨 **Rich Terminal UI**

  * Colored rendering using ANSI escape codes
  * Smooth controls (WASD + Arrow keys)

---

## 🕹️ Controls

| Key   | Action     |
| ----- | ---------- |
| W / ↑ | Move Up    |
| S / ↓ | Move Down  |
| A / ← | Move Left  |
| D / → | Move Right |
| Q     | Quit Game  |

---
## 🎥 Demo

▶️ Watch the demo: https://youtube.com/shorts/viaqSNIYwJI?feature=share

---

## 🛠️ Installation & Run

### Step 1: Clone the repository

```bash
git clone https://github.com/your-username/labyrinth-game.git
cd labyrinth-game
```

### Step 2: Compile

```bash
gcc -o labyrinth maze_game.c
```

### Step 3: Run

```bash
./labyrinth
```

---

## 📂 Project Structure

```
labyrinth-game/
│── maze_game.c        # Main game source code
│── README.md          # Project documentation
│── .labyrinth_scores  # Auto-generated scoreboard file
```

---

## ⚙️ Requirements

* GCC compiler
* Linux / macOS terminal (recommended)
* ANSI-compatible terminal for colors

> ⚠️ Windows users: Use WSL or Git Bash for best experience

---

## 🚀 Future Improvements

* Sound effects 🔊
* Multiple ghosts with AI behavior
* Dynamic maze generation
* Save/Load game progress
* Multiplayer mode


---
