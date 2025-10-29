#!/bin/bash
set -e

echo "[LXsystem] Sauvegarde du projet dans ./backup-lxsystem ..."
mkdir -p ./backup-lxsystem

# Copie le projet sans copier la sauvegarde elle-même
rsync -a --info=progress2 --exclude 'backup-lxsystem' ./ ./backup-lxsystem/

echo "[LXsystem] Remplacement de SerenityOS par LXsystem..."
# Remplacement dans tous les fichiers texte sauf binaires/images
find . -type f \
  ! -name "*.png" \
  ! -name "*.jpg" \
  ! -name "*.jpeg" \
  ! -name "*.ico" \
  ! -name "*.bmp" \
  ! -path "./backup-lxsystem/*" \
  -exec sed -i 's/SerenityOS/LXsystem/g' {} +

echo "[LXsystem] Nettoyage des fichiers de build..."
find . -type f \( -name "*.o" -o -name "*.obj" -o -name "*.exe" -o -name "*.iso" \) -delete 2>/dev/null || true

echo "[LXsystem] Terminé."
echo "Le projet a été sauvegardé dans ./backup-lxsystem"

