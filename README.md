# 📡 Simulation de Réseau Domestique IoT & Génération de Dataset ML (ns-3)

Ce dépôt contient le code source et les outils développés pour simuler un réseau domestique connecté (Smart Home) sous **ns-3**. L'objectif est de générer des traces réseaux réalistes et étiquetées pour entraîner des modèles de Machine Learning (Classification de trafic).

## 📝 Contexte du Projet

Dans un scénario de maison connectée comportant $N$ équipements de $K$ types différents (Caméras, Capteurs IoT, Ordinateurs, Téléphones VoIP), ce projet vise à :
1.  **Simuler** le trafic réseau généré par ces applications via Wifi 802.11ac.
2.  **Introduire de la variabilité** (spatiale, temporelle et physique) pour éviter le déterminisme.
3.  **Capturer et Transformer** les traces brutes (.pcap) en un Dataset structuré (.csv) utilisable par des algorithmes de ML.

## 📂 Structure du Dépôt

- **`/workspace/ns-allinone-3.45/ns-3-dev/scratch`** : Contient les scripts de simulation C++ pour ns-3.
  - `wifi-data-generation4.cc` : **Version finale** (Variabilité complète, Nakagami Fading, Logs).
  - `wifi-data-generation[1-3].cc` : Versions intermédiaires (Topologie, Capture, FlowMonitor).
- **`/workspace/analysis`** : Outils de traitement de données.
  - `pcap_to_dataset_final.py` : Script Python de Feature Engineering (Fenêtrage temporel, calcul IAT).
  - `pcap_to_dataset.py` : Script Python qui convertit chaque paquet capturé en une ligne de données CSV.
- **`/workspace/ns-allinone-3.45/ns-3-dev`** : Sortie des simulations.
  - `maison_animation4.xml` : Fichier de visualization d'animation final.
  - `wifi-traces-25-1.pcap` : Fichier de capture de trace final.
- **`/workspace/analysis`** : Échantillons de résultats.
  - `dataset_final.csv` : Dataset généré.
  
