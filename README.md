# 🕶️ Project Lens
> Smart glasses with real-time multilingual translation — breaking language barriers, one conversation at a time.

![Status](https://img.shields.io/badge/status-active-brightgreen)
![Type](https://img.shields.io/badge/type-hardware%20%2B%20ML-blue)
![Version](https://img.shields.io/badge/version-v0.1--planning-orange)

---

## About

Project Lens is a senior independent study building wearable smart glasses capable of translating live conversations across languages. The system combines embedded hardware, on-device or edge speech recognition, neural machine translation, and audio output — enabling two people speaking different languages to communicate fluidly without picking up their phones.

---

## Core Features

| Feature | Description |
|---|---|
| 🎙️ Live STT | Continuous speech-to-text from integrated microphones |
| 🌐 Neural Translation | Low-latency multilingual translation at the edge or via API |
| 🔊 Audio Output | Translated speech via bone conduction or earpiece |
| 👓 Minimal UX | HUD overlay or audio-only — no hands required |

---

## Repo Structure

```
project-lens/
├── firmware/           # microcontroller code
├── ml-experiments/     # model configs & benchmark results
├── hardware/           # BOM, schematics, CAD files
├── research/           # papers & annotated notes
├── logs/               # session engineering logs (YYYY-MM-DD.md)
├── reflections/        # personal daily reflections (YYYY-MM-DD.md)
├── decisions/          # architecture decision records (ADRs)
├── media/              # photos, videos, demos
├── todo/               # task tracking
├── ROADMAP.md
└── README.md
```

---

## Planned Stack

- **Hardware:** Raspberry Pi Zero 2W / ESP32
- **Speech-to-Text:** OpenAI Whisper (edge)
- **Translation:** Helsinki NLP / Meta NLLB
- **Runtime:** Python, TFLite / ONNX
- **Audio Output:** Bone conduction transducer
- **Frame:** Custom 3D-printed

---

## Milestones

- [ ] **Phase 1** — Project setup, repo structure, literature review
- [ ] **Phase 2** — Hardware prototype v1 (frame + mic + speaker)
- [ ] **Phase 3** — STT + translation pipeline with latency benchmarks
- [ ] **Phase 4** — Integration into wearable form factor
- [ ] **Phase 5** — Live demo & final SIS presentation

---

## Session Log Format

Each work session gets a file in `logs/YYYY-MM-DD.md`:

```markdown
# Log — YYYY-MM-DD

## What I Did
## What Worked
## What Failed / Blockers
## Decisions Made (and why)
## Metrics / Data
## Next Session Goals
```

## Daily Reflection Format

Each day gets a file in `reflections/YYYY-MM-DD.md`:

```markdown
# Reflection — YYYY-MM-DD

## Energy & Focus (1–10):
## What slowed me down today?
## What did I avoid, and why?
## One thing I'm proud of:
## Tomorrow's single top priority:
```

---

*Senior Independent Study · 2026*
