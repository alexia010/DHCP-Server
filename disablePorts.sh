#!/bin/bash

# Funcție pentru afișarea unui mesaj de eroare și oprirea scriptului
error_exit() {
    echo "[Eroare] $1" >&2
    exit 1
}

# Verificare dacă scriptul rulează cu privilegii root
if [[ $EUID -ne 0 ]]; then
    error_exit "Acest script trebuie rulat cu drepturi de root. Folosește 'sudo'."
fi

# Dezactivarea serviciilor DHCP implicite
echo "[Info] Dezactivez serviciile DHCP implicite..."
SERVICES=(isc-dhcp-server dhclient)
for SERVICE in "${SERVICES[@]}"; do
    if systemctl is-active --quiet $SERVICE; then
        echo "[Info] Oprez serviciul $SERVICE..."
        systemctl stop $SERVICE || error_exit "Nu am reușit să opresc serviciul $SERVICE."
    fi
    if systemctl is-enabled --quiet $SERVICE; then
        echo "[Info] Dezactivez serviciul $SERVICE..."
        systemctl disable $SERVICE || error_exit "Nu am reușit să dezactivez serviciul $SERVICE."
    fi
    echo "[Info] Serviciul $SERVICE a fost dezactivat."
done

# Eliminarea regulilor firewall pentru porturile 67 și 68
echo "[Info] Elimin regulile firewall pentru porturile 67 și 68..."
ufw delete deny 67/udp || echo "[Avertisment] Nu am reușit să elimin regula pentru portul 67."
ufw delete deny 68/udp || echo "[Avertisment] Nu am reușit să elimin regula pentru portul 68."
echo "[Info] Regulile firewall pentru porturile 67 și 68 au fost eliminate."

# Adăugarea permisiunilor speciale pentru aplicația personalizată
APPLICATION_PATH="/home/larisa/Desktop/gitFinal/DHCP-Server/dhcp_server/server"
if [[ -f "$APPLICATION_PATH" ]]; then
    echo "[Info] Setez permisiunile speciale pentru aplicație ($APPLICATION_PATH)..."
    setcap 'cap_net_bind_service=+ep' "$APPLICATION_PATH" || error_exit "Nu am reușit să setez permisiunile pentru aplicație."
    echo "[Info] Permisiunile speciale au fost setate pentru aplicație."
else
    echo "[Avertisment] Aplicația nu a fost găsită la calea specificată ($APPLICATION_PATH). Te rog să verifici."
fi

# Pornirea aplicației personalizate
# echo "[Info] Pornesc aplicația personalizată..."
# if [[ -f "$APPLICATION_PATH" ]]; then
#     sudo "$APPLICATION_PATH" 
#     if [[ $? -eq 0 ]]; then
#         echo "[Succes] Aplicația a fost pornită cu succes."
#     else
#         error_exit "Nu am reușit să pornesc aplicația."
#     fi
# else
#     error_exit "Aplicația nu a fost găsită la calea specificată ($APPLICATION_PATH). Te rog să verifici."
# fi

# Finalizare
echo "[Succes] Configurația pentru aplicația personalizată a fost finalizată."
exit 0
