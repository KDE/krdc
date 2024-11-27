# KRDC - KDE Remote Desktop Client

<p align="center">
  <img src="logo.png" alt="KRDC Logo" width="200">
</p>

KRDC is a remote desktop client for the KDE Plasma Desktop environment. It supports protocols like RDP and VNC, allowing users to connect to remote machines seamlessly. KRDC provides a user-friendly interface for accessing remote desktops, making it a powerful tool for remote administration or accessing your own machines from afar.

For more detailed information, including user guides and technical documentation, check out the official KDE resources:
- [KRDC Overview on KDE Apps](https://apps.kde.org/krdc/)
- [KRDC User Documentation](https://docs.kde.org/stable5/en/krdc/krdc/index.html)

---

## Features

- **Multi-Protocol Support:** Connect effortlessly using industry-standard protocols, including **RDP** (Remote Desktop Protocol) and **VNC** (Virtual Network Computing).
- **KDE Integration:** Seamlessly integrates with the KDE Plasma Desktop environment, providing a consistent and user-friendly experience.
- **Session Management:** Easily manage and revisit your remote connections with session history and quick access to recent connections.
- **Enhanced Security:** Secure your remote sessions with configurable encryption options, ensuring your data remains protected.

---

## Installation

### Using Package Manager

#### Debian-based systems:
```bash
sudo apt-get install krdc
```

#### openSUSE:
```bash
sudo zypper install krdc
```

### Building from Source
```bash
git clone https://invent.kde.org/network/krdc.git
cd krdc
mkdir build && cd build
cmake ..
make
sudo make install
```

---

## Usage

1. **Launch KRDC:**
   ```bash
   krdc
   ```

2. **Connect to a remote machine:**
   - Enter the remote host address (e.g., `rdp://192.168.1.10` or `vnc://hostname`).
   - Choose the connection protocol (RDP or VNC).
   - Click **Connect**.

3. **Session Management:**
   - KRDC keeps a history of recent connections for quick access.
   - You can bookmark favorite connections for easy management.

---

## Contributing

We welcome contributions from the community! Here's how you can help:

1. **Fork the repository** and clone it locally.
2. Create a new branch for your feature or bugfix.
3. **Submit a merge request (MR)** with a clear description of your changes.

For detailed contribution guidelines, please refer to [KDE's Contribution Guide](https://community.kde.org/Get_Involved/development).

---

## License

KRDC is distributed under the **GNU General Public License (GPL) 2.0 or later**. This covers most of the KRDC source code. See [LICENSES/GPL-2.0-or-later.txt](LICENSES/GPL-2.0-or-later.txt) for details.

For more information, please review each license file in the repository.

---

## Community & Support

- [KRDC Project Page](https://invent.kde.org/network/krdc)
- [KDE Community Forum](https://forum.kde.org/)
- [KDE Bug Tracker](https://bugs.kde.org/)
