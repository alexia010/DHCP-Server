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

# Reactivarea serviciilor DHCP implicite
echo "[Info] Reactivez serviciile DHCP implicite..."
SERVICES=(isc-dhcp-server dhclient)
for SERVICE in "${SERVICES[@]}"; do
    if ! systemctl is-enabled --quiet $SERVICE; then
        echo "[Info] Activez serviciul $SERVICE..."
        systemctl enable $SERVICE || error_exit "Nu am reușit să activez serviciul $SERVICE."
    fi
    echo "[Info] Pornez serviciul $SERVICE..."
    systemctl start $SERVICE || error_exit "Nu am reușit să pornesc serviciul $SERVICE."
done

# Configurarea firewall-ului pentru a bloca porturile 67 și 68 (dacă au fost deschise)
echo "[Info] Configurez firewall-ul pentru a bloca porturile 67 și 68..."
ufw deny 67/udp || error_exit "Nu am reușit să blochez portul 67."
ufw deny 68/udp || error_exit "Nu am reușit să blochez portul 68."
echo "[Info] Porturile 67 și 68 sunt blocate."


# Eliminarea permisiunilor speciale pentru aplicația personalizată
APPLICATION_PATH="/home/larisa/Desktop/gitFinal/DHCP-Server/dhcp_server/server"
if [[ -f "$APPLICATION_PATH" ]]; then
    echo "[Info] Elimin permisiunile speciale pentru aplicație ($APPLICATION_PATH)..."
    setcap -r "$APPLICATION_PATH" || error_exit "Nu am reușit să elimin permisiunile pentru aplicație."
    echo "[Info] Permisiunile speciale pentru aplicație au fost eliminate."
else
    echo "[Avertisment] Aplicația nu a fost găsită la calea specificată ($APPLICATION_PATH). Te rog să verifici."
fi

# Finalizare
echo "[Succes] Configurația implicită pentru porturile 67 și 68 a fost restaurată."
exit 0
