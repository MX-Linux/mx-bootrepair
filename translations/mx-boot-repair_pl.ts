<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="pl">
<context>
    <name>MainWindow</name>
    <message>
        <location filename="../mainwindow.ui" line="20"/>
        <source>MX Boot Repair</source>
        <translation>MX Naprawa rozruchu</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="45"/>
        <source>MX Boot Repair is a utility that can be used to reinstall GRUB bootloader on the ESP (EFI System Partition), MBR (Master Boot Record) or root partition. It provides the option to reconstruct the GRUB configuration file and to back up and restore MBR or PBR (root).</source>
        <translation>MX Naprawa rozruchu to narzędzie, za pomocą którego można ponownie zainstalować program rozruchowy GRUB na ESP (partycja systemowa EFI), na MBR (Master Boot Record - główny rekord rozruchowy) lub na partycji głównej. Zapewnia opcję odtworzenia pliku konfiguracyjnego GRUB oraz tworzenia kopii zapasowych i przywracania MBR lub PBR (root).</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="64"/>
        <source>What would you like to do?</source>
        <translation>Co chciałbyś zrobić?</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="94"/>
        <source>Backup MBR or PBR (legacy boot only)</source>
        <translation>Wykonaj kopię zapasową MBR lub PBR (tylko starsze komputery)</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="70"/>
        <source>Reinstall GRUB bootloader on ESP, MBR or PBR (root)</source>
        <translation>Zainstaluj ponownie program rozruchowy GRUB na ESP, MBR lub PBR (root)</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="80"/>
        <source>Repair GRUB configuration file</source>
        <translation>Napraw plik konfiguracyjny GRUB</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="87"/>
        <source>Regenerate initramfs images</source>
        <translation>Wygeneruj ponownie obrazy initramfs</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="101"/>
        <source>Restore MBR or PBR from backup (legacy boot only)</source>
        <translation>Przywróć MBR lub PBR z kopii zapasowej (tylko starsza wersja rozruchowa)</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="134"/>
        <location filename="../mainwindow.cpp" line="591"/>
        <source>Select Boot Method</source>
        <translation>Wybierz metodę rozruchu</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="152"/>
        <source>Master Boot Record</source>
        <translation>Master Boot Record (główny sektor rozruchowy)</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="155"/>
        <source>MBR</source>
        <translation>MBR</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="158"/>
        <location filename="../mainwindow.ui" line="398"/>
        <source>Alt+B</source>
        <translation>Alt+B</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="193"/>
        <location filename="../mainwindow.cpp" line="592"/>
        <source>Location:</source>
        <translation>Lokalizacja:</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="218"/>
        <source>EFI System Partition</source>
        <translation>Partycja systemowa EFI</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="221"/>
        <source>ESP</source>
        <translation>ESP</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="240"/>
        <source>Root (Partition Boot Record)</source>
        <translation>Root (Partycja rozruchowa)</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="243"/>
        <location filename="../mainwindow.cpp" line="594"/>
        <source>root</source>
        <translation>root</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="278"/>
        <location filename="../mainwindow.cpp" line="599"/>
        <source>Select root location:</source>
        <translation>Wybierz lokalizację katalogu głównego root:</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="300"/>
        <location filename="../mainwindow.cpp" line="593"/>
        <source>Install on:</source>
        <translation>Zainstaluj na:</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="389"/>
        <source>About this application</source>
        <translation>O programie</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="392"/>
        <source>About...</source>
        <translation>O...</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="440"/>
        <source>Display help </source>
        <translation>Wyświetl pomoc</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="443"/>
        <source>Help</source>
        <translation>Pomoc</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="449"/>
        <source>Alt+H</source>
        <translation>Alt+H</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="490"/>
        <source>Cancel any changes then quit</source>
        <translation>Anuluj wszelkie zmiany, a następnie zakończ</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="493"/>
        <source>Cancel</source>
        <translation>Anuluj</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="499"/>
        <source>Alt+N</source>
        <translation>Alt+N</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="518"/>
        <source>Apply any changes</source>
        <translation>Zastosuj wszystkie zmiany</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="521"/>
        <location filename="../mainwindow.cpp" line="79"/>
        <source>Next</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="627"/>
        <source>Apply</source>
        <translation>Zastosuj</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="94"/>
        <source>GRUB is being installed on %1 device.</source>
        <translation>GRUB jest instalowany na urządzeniu %1</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="133"/>
        <location filename="../mainwindow.cpp" line="216"/>
        <location filename="../mainwindow.cpp" line="271"/>
        <location filename="../mainwindow.cpp" line="389"/>
        <location filename="../mainwindow.cpp" line="481"/>
        <location filename="../mainwindow.cpp" line="519"/>
        <location filename="../mainwindow.cpp" line="632"/>
        <location filename="../mainwindow.cpp" line="636"/>
        <location filename="../mainwindow.cpp" line="643"/>
        <location filename="../mainwindow.cpp" line="649"/>
        <location filename="../mainwindow.cpp" line="657"/>
        <location filename="../mainwindow.cpp" line="664"/>
        <location filename="../mainwindow.cpp" line="716"/>
        <location filename="../mainwindow.cpp" line="727"/>
        <source>Error</source>
        <translation>Błąd</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="134"/>
        <location filename="../mainwindow.cpp" line="217"/>
        <location filename="../mainwindow.cpp" line="272"/>
        <source>Could not set up chroot environment.
Please double-check the selected location.</source>
        <translation>Błąd tworzenia środowiska chroot
Proszę sprawdź wybraną lokalizację docelową.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="197"/>
        <source>The GRUB configuration file (grub.cfg) is being rebuilt.</source>
        <translation>Plik konfiguracyjny GRUB (grub.cfg) jest przebudowywany.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="233"/>
        <source>Generating initramfs images on: %1</source>
        <translation>Generowanie obrazów initramfs na: %1</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="285"/>
        <source>Backing up MBR or PBR from %1 device.</source>
        <translation>Tworzenie kopii zapasowej MBR lub PBR z urządzenia %1</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="361"/>
        <source>Warning</source>
        <translation>Ostrzeżenie</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="362"/>
        <source>You are going to write the content of </source>
        <translation>Za chwilę nastąpi zapisanie zawartości </translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="362"/>
        <source> to </source>
        <translation>do</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="363"/>
        <source>

Are you sure?</source>
        <translation>

Jesteś pewien?</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="369"/>
        <source>Restoring MBR/PBR from backup to %1 device.</source>
        <translation>Przywracanie MBR/PBR z kopii zapasowej na urządzenie %1</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="390"/>
        <source>Could not find EFI system partition (ESP) on any system disks. Please create an ESP and try again.</source>
        <translation>Nie można znaleźć partycji systemowej EFI (ESP) na żadnym dysku systemowym. Utwórz ESP i spróbuj ponownie.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="433"/>
        <source>Select %1 location:</source>
        <translation>Wybierz lokalizację %1 :</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="458"/>
        <source>Back</source>
        <translation>Wstecz</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="475"/>
        <source>Success</source>
        <translation>Sukces </translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="475"/>
        <source>Process finished with success.&lt;p&gt;&lt;b&gt;Do you want to exit MX Boot Repair?&lt;/b&gt;</source>
        <translation>Proces zakończył się sukcesem. &lt;p&gt;Czy chcesz opuścić MX Naprawa rozruchu?&lt;/b&gt;</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="481"/>
        <source>Process finished. Errors have occurred.</source>
        <translation>Proces zakończony. Wystąpiły błędy.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="508"/>
        <source>Enter password to unlock %1 encrypted partition:</source>
        <translation>Wprowadź hasło, aby odblokować zaszyfrowaną partycję %1 :</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="519"/>
        <source>Sorry, could not open %1 LUKS container</source>
        <translation>Niestety, nie można otworzyć kontenera LUKS %1</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="598"/>
        <source>Select GRUB location</source>
        <translation>Wybierz lokalizację GRUB</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="607"/>
        <source>Select initramfs options</source>
        <translation>Wybierz opcje initramfs</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="617"/>
        <source>Select Item to Back Up</source>
        <translation>Wybierz element do utworzenia kopii zapasowej</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="622"/>
        <source>Select Item to Restore</source>
        <translation>Wybierz element do przywrócenia</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="632"/>
        <location filename="../mainwindow.cpp" line="643"/>
        <source>No location was selected.</source>
        <translation>Nie wybrano lokalizacji.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="637"/>
        <location filename="../mainwindow.cpp" line="650"/>
        <source>Please select the root partition of the system you want to fix.</source>
        <translation>Wybierz partycję główną systemu, który chcesz naprawić.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="655"/>
        <source>Select backup file name</source>
        <translation>Wybierz nazwę pliku kopii zapasowej</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="657"/>
        <location filename="../mainwindow.cpp" line="664"/>
        <source>No file was selected.</source>
        <translation>Nie wybrano pliku.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="662"/>
        <source>Select MBR or PBR backup file</source>
        <translation>Wybierz plik kopii zapasowej MBR lub PBR</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="680"/>
        <source>About %1</source>
        <translation>O %1</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="682"/>
        <source>Version: </source>
        <translation>Wersja:</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="683"/>
        <source>Simple boot repair program for MX Linux</source>
        <translation>Prosty program do naprawy rozruchu dla dystrybucji antiX MX</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="685"/>
        <source>Copyright (c) MX Linux</source>
        <translation>Prawa autorskie © MX Linux</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="686"/>
        <source>%1 License</source>
        <translation>%1 Licencja</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="699"/>
        <source>%1 Help</source>
        <translation>%1 Pomoc</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="716"/>
        <source>Sorry, could not mount %1 partition</source>
        <translation>Niestety, nie można zamontować partycji %1</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="727"/>
        <source>Could not create a temporary folder</source>
        <translation>Nie można utworzyć folderu tymczasowego</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <location filename="../about.cpp" line="52"/>
        <source>License</source>
        <translation>Licencja</translation>
    </message>
    <message>
        <location filename="../about.cpp" line="53"/>
        <location filename="../about.cpp" line="63"/>
        <source>Changelog</source>
        <translation>Dziennik zmian</translation>
    </message>
    <message>
        <location filename="../about.cpp" line="54"/>
        <source>Cancel</source>
        <translation>Anuluj</translation>
    </message>
    <message>
        <location filename="../about.cpp" line="73"/>
        <source>&amp;Close</source>
        <translation>&amp;Zamknij</translation>
    </message>
    <message>
        <location filename="../main.cpp" line="53"/>
        <source>MX Boot Repair</source>
        <translation>MX Naprawa rozruchu</translation>
    </message>
    <message>
        <location filename="../main.cpp" line="86"/>
        <source>Error</source>
        <translation>Błąd</translation>
    </message>
    <message>
        <location filename="../main.cpp" line="87"/>
        <source>You must run this program with admin access.</source>
        <translation>Musisz uruchomić ten program z uprawnieniami administratora.</translation>
    </message>
</context>
</TS>
