<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="ru">
<context>
    <name>MainWindow</name>
    <message>
        <location filename="../mainwindow.ui" line="20"/>
        <source>MX Boot Repair</source>
        <translation>MX Восстановление загрузчика</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="39"/>
        <source>MX Boot Repair is a utility that can be used to reinstall GRUB bootloader on the ESP (EFI System Partition), MBR (Master Boot Record) or root partition. It provides the option to reconstruct the GRUB configuration file and to back up and restore MBR or PBR (root).</source>
        <translation>MX Ремонт Загрузки - это утилита позволяющая переустановить загрузчик GRUB в ESP (системный раздел EFI), MBR (главная загрузочная запись) или корневой раздел. Она имеет опцию пересоздания конфигурационного файла GRUB и сохранения копии или восстановления MBR или PBR (корневой).</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="58"/>
        <source>What would you like to do?</source>
        <translation>Что Вы желаете сделать?</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="79"/>
        <source>Backup MBR or PBR (legacy boot only)</source>
        <translation>Резервное копирование MBR или PBR (только если загрузка существует)</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="86"/>
        <source>Reinstall GRUB bootloader on ESP, MBR or PBR (root)</source>
        <translation>Переустановить загрузчик GRUB в ESP, MBR или PBR (корневой)</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="96"/>
        <source>Repair GRUB configuration file</source>
        <translation>Исправить файл конфигурации GRUB</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="103"/>
        <source>Restore MBR or PBR from backup (legacy boot only)</source>
        <translation>Восстановить MBR или PBR из резервной копии (только если загрузка существует)</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="136"/>
        <location filename="../mainwindow.cpp" line="536"/>
        <source>Select Boot Method</source>
        <translation>Выбрать метод загрузки</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="154"/>
        <source>Master Boot Record</source>
        <translation>Главная загрузочная запись</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="157"/>
        <source>MBR</source>
        <translation>MBR</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="160"/>
        <location filename="../mainwindow.ui" line="430"/>
        <source>Alt+B</source>
        <translation>Alt+B</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="182"/>
        <source>Root (Partition Boot Record)</source>
        <translation>Root (Загрузочная запись на разделе)</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="185"/>
        <location filename="../mainwindow.cpp" line="539"/>
        <source>root</source>
        <translation>корневой раздел</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="236"/>
        <location filename="../mainwindow.cpp" line="538"/>
        <source>Install on:</source>
        <translation>Установить на:</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="255"/>
        <location filename="../mainwindow.cpp" line="537"/>
        <source>Location:</source>
        <translation>Местоположение:</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="277"/>
        <location filename="../mainwindow.cpp" line="547"/>
        <source>Select root location:</source>
        <translation>Корневая файловая система:</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="290"/>
        <source>EFI System Partition</source>
        <translation>Системный раздел EFI</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="293"/>
        <source>ESP</source>
        <translation>ESP</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="420"/>
        <source>About this application</source>
        <translation>Об этом приложении</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="423"/>
        <source>About...</source>
        <translation>О программе...</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="472"/>
        <source>Display help </source>
        <translation>Показать справку</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="475"/>
        <source>Help</source>
        <translation>Справка</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="482"/>
        <source>Alt+H</source>
        <translation>Alt+H</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="523"/>
        <source>Cancel any changes then quit</source>
        <translation>Отменить все изменения, затем выйти</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="526"/>
        <source>Cancel</source>
        <translation>Отменить</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="533"/>
        <source>Alt+N</source>
        <translation>Alt+N</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="552"/>
        <source>Apply any changes</source>
        <translation>Применить все изменения</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="555"/>
        <location filename="../mainwindow.cpp" line="79"/>
        <source>Apply</source>
        <translation>Применить</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="94"/>
        <source>GRUB is being installed on %1 device.</source>
        <translation>GRUB устанавливается в устройство %1.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="119"/>
        <location filename="../mainwindow.cpp" line="148"/>
        <location filename="../mainwindow.cpp" line="213"/>
        <location filename="../mainwindow.cpp" line="245"/>
        <location filename="../mainwindow.cpp" line="354"/>
        <location filename="../mainwindow.cpp" line="449"/>
        <location filename="../mainwindow.cpp" line="473"/>
        <location filename="../mainwindow.cpp" line="578"/>
        <location filename="../mainwindow.cpp" line="585"/>
        <location filename="../mainwindow.cpp" line="642"/>
        <source>Error</source>
        <translation>Ошибка</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="119"/>
        <location filename="../mainwindow.cpp" line="213"/>
        <source>Could not create a temporary folder</source>
        <translation>Не удалось создать временную папку</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="148"/>
        <location filename="../mainwindow.cpp" line="245"/>
        <source>Could not set up chroot environment.
Please double-check the selected location.</source>
        <translation>Не удалось установить среду chroot.
Пожалуйста, перепроверьте выбранное местоположение.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="210"/>
        <source>The GRUB configuration file (grub.cfg) is being rebuilt.</source>
        <translation>Конфигурационный файл GRUB (grub.cfg) перестраивается.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="261"/>
        <source>Backing up MBR or PBR from %1 device.</source>
        <translation>Создание резервной копии MBR или PBR с устройства %1.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="327"/>
        <source>Warning</source>
        <translation>Предупреждение</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="328"/>
        <source>You are going to write the content of </source>
        <translation>Вы собираетесь записать содержимое</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="329"/>
        <source> to </source>
        <translation>на</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="329"/>
        <source>

Are you sure?</source>
        <translation>

Вы уверены?</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="334"/>
        <source>Restoring MBR/PBR from backup to %1 device.</source>
        <translation>Восстановление MBR/PBR на устройстве %1 из резервной копии.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="354"/>
        <source>Could not find EFI system partition (ESP) on any system disks. Please create an ESP and try again.</source>
        <translation>Не удалось обнаружить системный раздел EFI (ESP) на всех дисках. Пожалуйста, создайте ESP и повторите попытку.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="396"/>
        <source>Select %1 location:</source>
        <translation>Выберите расположение %1:</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="428"/>
        <source>Back</source>
        <translation>Назад</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="444"/>
        <source>Success</source>
        <translation>Успешно</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="445"/>
        <source>Process finished with success.&lt;p&gt;&lt;b&gt;Do you want to exit MX Boot Repair?&lt;/b&gt;</source>
        <translation>Процесс завершился успешно.&lt;p&gt;&lt;b&gt;Вы хотите выйти из MX Восстановление загрузчика?&lt;/b&gt;</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="449"/>
        <source>Process finished. Errors have occurred.</source>
        <translation>Процесс завершен. Имеются ошибки.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="466"/>
        <source>Enter password to unlock %1 encrypted partition:</source>
        <translation>Введите пароль для открытия зашифрованного раздела %1:</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="473"/>
        <source>Sorry, could not open %1 LUKS container</source>
        <translation>Извините, не получилось открыть контейнер LUKS %1</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="546"/>
        <source>Select GRUB location</source>
        <translation>Выберите расположение GRUB</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="558"/>
        <source>Select Item to Back Up</source>
        <translation>Выберите элемент для резервного копирования</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="565"/>
        <source>Select Item to Restore</source>
        <translation>Выберите элемент для восстановления</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="576"/>
        <source>Select backup file name</source>
        <translation>Выберите имя файла резервной копии</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="578"/>
        <location filename="../mainwindow.cpp" line="585"/>
        <source>No file was selected.</source>
        <translation>Файл не выбран.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="583"/>
        <source>Select MBR or PBR backup file</source>
        <translation>Выберите резервный файл MBR или PBR</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="602"/>
        <source>About %1</source>
        <translation>О %1</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="604"/>
        <source>Version: </source>
        <translation>Версия: </translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="605"/>
        <source>Simple boot repair program for MX Linux</source>
        <translation>Простая программа для восстановления загрузчика в MX Linux</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="607"/>
        <source>Copyright (c) MX Linux</source>
        <translation>Copyright (c) MX Linux</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="608"/>
        <source>%1 License</source>
        <translation>%1 Лицензия</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="619"/>
        <source>%1 Help</source>
        <translation>%1 Справка</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="642"/>
        <source>Sorry, could not mount %1 partition</source>
        <translation>Простите, не удалось примонтировать раздел %1</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <location filename="../about.cpp" line="50"/>
        <source>License</source>
        <translation>Лицензия</translation>
    </message>
    <message>
        <location filename="../about.cpp" line="51"/>
        <location filename="../about.cpp" line="60"/>
        <source>Changelog</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../about.cpp" line="52"/>
        <source>Cancel</source>
        <translation type="unfinished">Отменить</translation>
    </message>
    <message>
        <location filename="../about.cpp" line="70"/>
        <source>&amp;Close</source>
        <translation>&amp;Закрыть</translation>
    </message>
    <message>
        <location filename="../main.cpp" line="65"/>
        <source>Error</source>
        <translation type="unfinished">Ошибка</translation>
    </message>
    <message>
        <location filename="../main.cpp" line="66"/>
        <source>You seem to be logged in as root, please log out and log in as normal user to use this program.</source>
        <translation type="unfinished"></translation>
    </message>
</context>
</TS>
