# CheatNote 🚀📝


![Platform](https://img.shields.io/badge/platform-linux%20%7C%20macos%20%7C%20windows-blue)

---

CheatNote is your **blazing-fast, portable command-line companion** for managing code snippets, programming notes, and technical knowledge — all from your terminal! ✨  

---

## Why CheatNote?  

- ⏳ **Save keystrokes** — no more re-Googling the same commands.  
- 🧠 **Remember everything** — instantly recall snippets, notes, commands.  
- ⚡ **Stay in flow** — keep your hands on the keyboard.  
- 🌍 **Portable & lightweight** — written in C, runs anywhere.  

Perfect for **developers, sysadmins, students, and power-users**. 💡⌨️  


<p align="center">
  <img src="https://media.giphy.com/media/26tPplGWjN0xLybiU/giphy.gif" alt="Search/List Demo" width="400"/>
</p>

---

## Installation 🔧  

CheatNote builds from source with a simple `make` system.  

### 1. Build  

```bash
# Clone the repo
git clone https://github.com/bug-free-dev/cheatnote.git
cd cheatnote

# Build release binary
make
```

The binary will be in `bin/cheatnote`.

---

### 2. Install (system-wide)

```bash
sudo make install
```

By default this installs to `/usr/local/bin/cheatnote`.
Now you can run it from anywhere:

```bash
cheatnote add "Hello" "My first note" "test"
```

---

### 3. Install (custom path)

```bash
make install PREFIX=$HOME/.local
```

This puts the binary into `~/.local/bin/cheatnote`.
Make sure `~/.local/bin` is in your `$PATH`:

```bash
export PATH="$HOME/.local/bin:$PATH"
```
---

### 4. Uninstall

```bash
sudo make uninstall
```

Or, if you installed with a custom prefix:

```bash
make uninstall PREFIX=$HOME/.local
```

---

# [DESIGN](docs/DESIGN.md)
Refer the DESIGN.md for more details.