import sys
from scapy.all import *
import pandas as pd
import numpy as np

# Dataset
# Serveur 0 (192.168.1.2) = Type 0 (IoT_Temperature)
# Serveur 1 (192.168.1.3) = Type 1 (Camera_Surveillance)
# Serveur 2 (192.168.1.4) = Type 2 (Web_Browsing)
# Serveur 3 (192.168.1.5) = Type 3 (VoIP)

IP_TO_LABEL = {
    "192.168.1.2": "IoT_Sensor",
    "192.168.1.3": "Camera_IP",
    "192.168.1.4": "Web_Traffic",
    "192.168.1.5": "VoIP_Call"
}

def pcap_to_csv(pcap_file, output_csv):
    print(f"[*] Lecture du fichier : {pcap_file} ...")
    
    data_list = []
    
    # On lit le pcap paquet par paquet (PcapReader)
    with PcapReader(pcap_file) as pcap_reader:
        for i, pkt in enumerate(pcap_reader):
            
            if i % 10000 == 0:
                print(f"    -> Traitement paquet {i}...")

            # On recupère les paquets IP
            if not pkt.haslayer(IP):
                continue
                
            ip_layer = pkt[IP]
            
            # Extraction des infos de base
            timestamp = float(pkt.time)
            src_ip = ip_layer.src
            dst_ip = ip_layer.dst
            packet_size = len(pkt) # Taille totale trame
            
            # Détermination du Protocole
            protocol = "OTHER"
            sport = 0
            dport = 0
            
            if pkt.haslayer(TCP):
                protocol = "TCP"
                sport = pkt[TCP].sport
                dport = pkt[TCP].dport
            elif pkt.haslayer(UDP):
                protocol = "UDP"
                sport = pkt[UDP].sport
                dport = pkt[UDP].dport
            
            # Labels
            # Si le paquet va VERS un serveur connu -> C'est une requête de ce type
            # Si le paquet vient D'UN serveur connu -> C'est une réponse de ce type
            label = "Unknown"
            
            if dst_ip in IP_TO_LABEL:
                label = IP_TO_LABEL[dst_ip]
                direction = "Upload" # Vers le serveur
            elif src_ip in IP_TO_LABEL:
                label = IP_TO_LABEL[src_ip]
                direction = "Download" # Depuis le serveur
            else:
                # Trafic entre noeuds ou vers AP (bruit ou control)
                continue 

            
            data_list.append({
                "timestamp": timestamp,
                "src_ip": src_ip,
                "dst_ip": dst_ip,
                "src_port": sport,
                "dst_port": dport,
                "protocol": protocol,
                "size_bytes": packet_size,
                "direction": direction,
                "label": label
            })

    print(f"[*] Conversion en DataFrame ({len(data_list)} paquets IP utiles trouvés)")
    df = pd.DataFrame(data_list)
    
    print(f"[*] Sauvegarde dans {output_csv}")
    df.to_csv(output_csv, index=False)
    print("[+] Terminé !")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 pcap_to_dataset.py <input.pcap> <output.csv>")
    else:
        pcap_to_csv(sys.argv[1], sys.argv[2])
