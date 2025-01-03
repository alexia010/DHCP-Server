-- database: ./config2.db


-- tabelul `network` pentru configurarile fiecarei retele și pool-ul de adrese IP
CREATE TABLE network (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    network_name TEXT NOT NULL UNIQUE,     
    ip_range_start TEXT NOT NULL,          
    ip_range_end TEXT NOT NULL,            
    ip_current TEXT NOT NULL,
    subnet_mask TEXT NOT NULL,            
    gateway TEXT NOT NULL,                           
    broadcast TEXT NOT NULL
);

-- tabelul `dns_servers` pentru stocarea adreselor serverelor DNS disponibile
CREATE TABLE dns_servers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    dns_ip TEXT NOT NULL UNIQUE           
);

-- tabelul de asociere `network_dns` pentru relatia many-to-many între retele și serverele DNS
CREATE TABLE network_dns (
    

    network_id INTEGER NOT NULL,          
    dns_id INTEGER NOT NULL,               
    

    FOREIGN KEY (network_id) REFERENCES network(id) ON DELETE CASCADE,
    FOREIGN KEY (dns_id) REFERENCES dns_servers(id) ON DELETE CASCADE,

    PRIMARY KEY (network_id, dns_id)      -- cheie primara compusă
);

-- tabelul `ip_mac_leases` pentru asocierile IP-MAC + informatiile despre lease
CREATE TABLE ip_mac_leases (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    network_id INTEGER NOT NULL,           

    ip_address TEXT NOT NULL UNIQUE,       -- adresa IP alocata
    mac_address TEXT NOT NULL UNIQUE,       -- adresa MAC a clientului
    lease DATETIME NOT NULL,         -- data și ora de inceput a lease-ului
    lease_end DATETIME NOT NULL,           -- data si ora de sfarsit a lease-ului
    is_static INTEGER NOT NULL DEFAULT 0,  -- 1 = alocare statica
                                           -- 0 = dinamică

    FOREIGN KEY (network_id) REFERENCES network(id) ON DELETE CASCADE
);


-- tabelul `mac_whitelist` pentru filtrarea pe baza adreselor MAC permise
CREATE TABLE mac_whitelist (
    id INTEGER PRIMARY KEY AUTOINCREMENT,

    mac_address TEXT NOT NULL UNIQUE,      -- adresa MAC a clientului permis
    description TEXT                       
);


INSERT INTO network (network_name, ip_range_start, ip_range_end, ip_current, subnet_mask, gateway, broadcast)
VALUES 
('ntw1', '192.168.1.100', '192.168.1.200', '192.168.1.100', '255.255.255.0', '192.168.1.1', '192.168.1.255'),
('ntw2', '10.0.0.100', '10.0.0.150', '10.0.0.100', '255.255.255.0', '10.0.0.1', '10.0.0.255'),
('ntw3', '172.16.0.50', '172.16.0.100', '172.16.0.50', '255.255.0.0', '172.16.0.1', '172.16.0.255');


INSERT INTO dns_servers (dns_ip)
VALUES 
('8.8.8.8'),
('8.8.4.4'),
('1.1.1.1');

INSERT INTO network_dns (network_id, dns_id)
VALUES 
(1, 1),  
(2, 2),  
(3, 3),
(3, 1);

INSERT INTO mac_whitelist (mac_address, description)
VALUES ('aa:bb:cc:dd:ee:ff', 'PC personal');