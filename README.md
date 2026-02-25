# BNP-project - File Monitor

Système de surveillance de fichiers en **C orienté objet**, sans bibliothèques tierces.
Utilise uniquement la **bibliothèque standard C** et l'API Linux **inotify** (sys/inotify.h).

## Fonctionnalités

- Surveillance des fichiers depuis la racine `/`
- Détection des modifications (write, create, delete, move, attrib)
- Log de tous les événements dans un fichier `.txt`
- Configuration via fichier `.yaml`
- Exclusions configurables (proc, sys, dev, etc.)

## Configuration (config.yaml)

```yaml
watch:
  - /                    # Chemins à surveiller

exclude:
  - /proc                # Chemins exclus
  - /sys
  - /dev
  - /run
  - /tmp

log_file: file_monitor.log
recursive: true
```

## Compilation

```bash
make or make -f Makefile.gnustep (obj C)
```

## Utilisation

```bash
# Avec config.yaml par défaut
./file_monitor or ./file_monitor_objc

# Avec un fichier de config spécifique
./file_monitor config.yaml

# Test local (sans root, surveille le répertoire courant)
make test-local
```

**Note :** Surveiller `/` requiert les droits root.

## Structure (C orienté objet)

- `Config` – parseur YAML minimal, configuration
- `Logger` – log des événements dans un fichier txt
- `FileMonitor` – moniteur inotify avec méthodes init/run/stop/destroy

## Format du log

```
[2025-02-22 14:30:00] MODIFY | /path/to/file | 
[2025-02-22 14:30:01] CREATE | /path/newfile | 
[2025-02-22 14:30:02] CLOSE_WRITE | /path/modified | 
```