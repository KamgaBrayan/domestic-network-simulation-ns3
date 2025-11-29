import sys
from scapy.all import *
import pandas as pd
import numpy as np

IP_TO_LABEL = {
    "192.168.1.2": "IoT_Sensor",
    "192.168.1.3": "Camera_IP",
    "192.168.1.4": "Web_Traffic",
    "192.168.1.5": "VoIP_Call"
}

def pcap_to_features(pcap_file, output_csv, window_size=1.0):
    print(f"[*] Lecture: {pcap_file}")
    
    # Dictionnaire pour stocker les derniers temps d'arrivée par flux (pour calcul IAT)
    # Clé: (src_ip, dst_ip, dst_port) -> Valeur: timestamp
    last_arrival_times = {}
    
    raw_data = []

    # Extraction brute et Calcul IAT
    with PcapReader(pcap_file) as pcap_reader:
        for pkt in pcap_reader:
            if not pkt.haslayer(IP): continue
            
            ip = pkt[IP]
            ts = float(pkt.time)
            
            # Filtrage des labels
            label = "Noise"
            if ip.dst in IP_TO_LABEL:
                label = IP_TO_LABEL[ip.dst]
                flow_id = (ip.src, ip.dst, ip.dst) # Sens montant
            elif ip.src in IP_TO_LABEL:
                label = IP_TO_LABEL[ip.src]
                flow_id = (ip.src, ip.dst, ip.src) # Sens descendant
            else:
                continue # On ignore le trafic de contrôle pur

            # Identification Protocol
            proto = 0 # Other
            size = len(pkt)
            if pkt.haslayer(TCP): proto = 6
            elif pkt.haslayer(UDP): proto = 17

            # Calcul IAT
            iat = 0.0
            if flow_id in last_arrival_times:
                iat = ts - last_arrival_times[flow_id]
            last_arrival_times[flow_id] = ts

            raw_data.append({
                'timestamp': ts,
                'flow_id': str(flow_id), # Pour grouper plus tard
                'size': size,
                'iat': iat,
                'proto': proto,
                'label': label
            })

    df = pd.DataFrame(raw_data)
    print(f"[*] Paquets extraits: {len(df)}")

    if len(df) == 0:
        print("Aucune donnée trouvée.")
        return

    # Aggregation par Fenêtres Temporelles (Feature Engineering)
    # On découpe le temps en tranches de 'window_size' secondes (ex: 1s)
    # Pour chaque flux dans chaque tranche, on calcule les stats.
    
    df['time_window'] = (df['timestamp'] // window_size).astype(int)
    
    # On groupe par (Fenêtre de temps, Flux, Label)
    grouped = df.groupby(['time_window', 'flow_id', 'label'])
    
    features_list = []
    
    for name, group in grouped:
        window_idx, flow, label = name
        
        # Calcul des statistiques 
        features = {
            'flow_id': flow,
            'window_id': window_idx,
            
            # Stats sur la taille des paquets
            'pkt_count': len(group),
            'size_mean': group['size'].mean(),
            'size_std': group['size'].std(ddof=0),
            'size_min': group['size'].min(),
            'size_max': group['size'].max(),
            'bytes_total': group['size'].sum(),
            
            # Stats sur les temps inter-arrivées (IAT)
            'iat_mean': group['iat'].mean(),
            'iat_std': group['iat'].std(ddof=0),
            'iat_max': group['iat'].max(),
            'iat_min': group['iat'].min(),
            
            # Protocole (Mode car constant par flux généralement)
            'proto': group['proto'].mode()[0],
            
            # Target
            'Label': label
        }
        features_list.append(features)

    df_final = pd.DataFrame(features_list)
    
    # Nettoyage des NaN (si un seul paquet, std est NaN -> on met 0)
    df_final.fillna(0, inplace=True)
    
    print(f"[*] Dataset final généré: {len(df_final)} échantillons (fenêtres)")
    print(f"[*] Sauvegarde: {output_csv}")
    df_final.to_csv(output_csv, index=False)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 pcap_to_dataset_final.py <input.pcap> <output.csv>")
    else:
        pcap_to_features(sys.argv[1], sys.argv[2])
