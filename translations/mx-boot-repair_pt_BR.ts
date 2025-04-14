<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="pt_BR">
<context>
    <name>MainWindow</name>
    <message>
        <location filename="../mainwindow.ui" line="20"/>
        <source>MX Boot Repair</source>
        <translation>Reparador de Inicialização do MX</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="45"/>
        <source>MX Boot Repair is a utility that can be used to reinstall GRUB bootloader on the ESP (EFI System Partition), MBR (Master Boot Record) or root partition. It provides the option to reconstruct the GRUB configuration file and to back up and restore MBR or PBR (root).</source>
        <translation>O Reparador de Inicialização do MX (MX Boot Repair) é um utilitário que pode ser utilizado para reinstalar ou reparar o gerenciador/carregador de inicialização GRUB na ESP (EFI System Partition), no MBR (Master Boot Record) ou na partição raiz ( /, root). O reparador fornece as opções de reconstruir o arquivo de configurações do GRUB, fazer cópia de segurança e restaurar o MBR ou o PBR (root).</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="64"/>
        <source>What would you like to do?</source>
        <translation>O que você pretende fazer?</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="94"/>
        <source>Backup MBR or PBR (legacy boot only)</source>
        <translation>Cópia de segurança do MBR ou do PBR (apenas em computadores com BIOS)</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="70"/>
        <source>Reinstall GRUB bootloader on ESP, MBR or PBR (root)</source>
        <translation>Reinstalar o gerenciador/carregador de inicialização GRUB em uma partição ESP, MBR ou PBR (root)</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="80"/>
        <source>Repair GRUB configuration file</source>
        <translation>Reparar o arquivo de configurações do GRUB</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="87"/>
        <source>Regenerate initramfs images</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="101"/>
        <source>Restore MBR or PBR from backup (legacy boot only)</source>
        <translation>Restaurar MBR ou PBR a partir de cópia (apenas em computadores com BIOS)</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="134"/>
        <location filename="../mainwindow.cpp" line="600"/>
        <source>Select Boot Method</source>
        <translation>Selecionar o Método de Inicialização</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="152"/>
        <source>Master Boot Record</source>
        <translation>Master Boot Record (Registro Mestre de Inicialização)</translation>
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
        <location filename="../mainwindow.cpp" line="601"/>
        <source>Location:</source>
        <translation>Localização:</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="218"/>
        <source>EFI System Partition</source>
        <translation>Partição de Sistema EFI</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="221"/>
        <source>ESP</source>
        <translation>ESP</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="240"/>
        <source>Root (Partition Boot Record)</source>
        <translation>Root (Partition Boot Record - Registro da Partição de Inicialização)</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="243"/>
        <location filename="../mainwindow.cpp" line="603"/>
        <source>root</source>
        <translation>raiz (PBR)</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="278"/>
        <location filename="../mainwindow.cpp" line="608"/>
        <source>Select root location:</source>
        <translation>Selecionar a localização da raiz ( /, root):</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="300"/>
        <location filename="../mainwindow.cpp" line="602"/>
        <source>Install on:</source>
        <translation>Instalar em:</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="389"/>
        <source>About this application</source>
        <translation>Sobre este aplicativo</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="392"/>
        <source>About...</source>
        <translation>Sobre...</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="440"/>
        <source>Display help </source>
        <translation>Exibir ajuda</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="443"/>
        <source>Help</source>
        <translation>Ajuda</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="449"/>
        <source>Alt+H</source>
        <translation>Alt+H</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="490"/>
        <source>Cancel any changes then quit</source>
        <translation>Cancelar as alterações e sair</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="493"/>
        <source>Cancel</source>
        <translation>Cancelar</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="499"/>
        <source>Alt+N</source>
        <translation>Alt+N</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="518"/>
        <source>Apply any changes</source>
        <translation>Aplicar as alterações</translation>
    </message>
    <message>
        <location filename="../mainwindow.ui" line="521"/>
        <location filename="../mainwindow.cpp" line="84"/>
        <source>Apply</source>
        <translation>Aplicar</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="99"/>
        <source>GRUB is being installed on %1 device.</source>
        <translation>O GRUB está sendo instalado em %1</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="138"/>
        <location filename="../mainwindow.cpp" line="221"/>
        <location filename="../mainwindow.cpp" line="278"/>
        <location filename="../mainwindow.cpp" line="398"/>
        <location filename="../mainwindow.cpp" line="490"/>
        <location filename="../mainwindow.cpp" line="528"/>
        <location filename="../mainwindow.cpp" line="639"/>
        <location filename="../mainwindow.cpp" line="643"/>
        <location filename="../mainwindow.cpp" line="650"/>
        <location filename="../mainwindow.cpp" line="656"/>
        <location filename="../mainwindow.cpp" line="664"/>
        <location filename="../mainwindow.cpp" line="671"/>
        <location filename="../mainwindow.cpp" line="723"/>
        <location filename="../mainwindow.cpp" line="734"/>
        <source>Error</source>
        <translation>Erro</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="139"/>
        <location filename="../mainwindow.cpp" line="222"/>
        <location filename="../mainwindow.cpp" line="279"/>
        <source>Could not set up chroot environment.
Please double-check the selected location.</source>
        <translation>Não foi possível configurar o ambiente ‘Chroot’ Change root).
Por favor, verifique novamente o local que foi selecionado.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="202"/>
        <source>The GRUB configuration file (grub.cfg) is being rebuilt.</source>
        <translation>O arquivo de configurações do GRUB (grub.cfg) está sendo reconstruído.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="239"/>
        <source>Generating initramfs images on: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="292"/>
        <source>Backing up MBR or PBR from %1 device.</source>
        <translation>Fazendo a cópia de segurança do MBR ou do PBR do dispositivo %1</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="370"/>
        <source>Warning</source>
        <translation>Aviso</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="371"/>
        <source>You are going to write the content of </source>
        <translation>Você vai escrever o conteúdo de</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="371"/>
        <source> to </source>
        <translation> em </translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="372"/>
        <source>

Are you sure?</source>
        <translation>
    
Você tem certeza?</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="378"/>
        <source>Restoring MBR/PBR from backup to %1 device.</source>
        <translation>Restaurando MBR/PBR para o dispositivo %1 a partir da cópia de segurança.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="399"/>
        <source>Could not find EFI system partition (ESP) on any system disks. Please create an ESP and try again.</source>
        <translation>Não foi possível encontrar a partição ESP (EFI System Partition) em qualquer dos discos do sistema. Crie uma ESP e tente novamente.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="442"/>
        <source>Select %1 location:</source>
        <translation>Selecionar a localização de %1:</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="467"/>
        <source>Back</source>
        <translation>Voltar</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="484"/>
        <source>Success</source>
        <translation>Sucesso</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="484"/>
        <source>Process finished with success.&lt;p&gt;&lt;b&gt;Do you want to exit MX Boot Repair?&lt;/b&gt;</source>
        <translation>O processo foi concluído com sucesso.&lt;p&gt;&lt;b&gt;Você quer sair do Reparador de Inicialização do MX (MX Boot Repair)?&lt;/b&gt;</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="490"/>
        <source>Process finished. Errors have occurred.</source>
        <translation>O processo foi concluído. Ocorreram alguns erros.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="517"/>
        <source>Enter password to unlock %1 encrypted partition:</source>
        <translation>Inserir a senha para desbloquear a partição criptografada/encriptada %1:</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="528"/>
        <source>Sorry, could not open %1 LUKS container</source>
        <translation>Desculpe, não foi possível abrir o contêiner %1 LUKS</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="607"/>
        <source>Select GRUB location</source>
        <translation>Selecione a localização do GRUB</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="616"/>
        <source>Select initramfs options</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="626"/>
        <source>Select Item to Back Up</source>
        <translation>Selecione o item para fazer a copiar de segurança</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="631"/>
        <source>Select Item to Restore</source>
        <translation>Selecione o item para restaurar</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="639"/>
        <location filename="../mainwindow.cpp" line="650"/>
        <source>No location was selected.</source>
        <translation>Nenhum local foi selecionado.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="644"/>
        <location filename="../mainwindow.cpp" line="657"/>
        <source>Please select the root partition of the system you want to fix.</source>
        <translation>Por favor, selecione a partição de ‘root’ (raiz) do sistema que você quer corrigir.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="662"/>
        <source>Select backup file name</source>
        <translation>Escolha o nome do arquivo da cópia de segurança (backup).</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="664"/>
        <location filename="../mainwindow.cpp" line="671"/>
        <source>No file was selected.</source>
        <translation>Nenhum arquivo foi selecionado.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="669"/>
        <source>Select MBR or PBR backup file</source>
        <translation>Selecione o arquivo da cópia de segurança (backup) MBR ou PBR</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="687"/>
        <source>About %1</source>
        <translation>Sobre o %1</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="689"/>
        <source>Version: </source>
        <translation>Versão: </translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="690"/>
        <source>Simple boot repair program for MX Linux</source>
        <translation>Programa simples de reparo de inicialização (boot) para o MX Linux.</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="692"/>
        <source>Copyright (c) MX Linux</source>
        <translation>Direitos de Autor (c) MX Linux</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="693"/>
        <source>%1 License</source>
        <translation>Licença do %1</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="706"/>
        <source>%1 Help</source>
        <translation>Ajuda do %1</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="723"/>
        <source>Sorry, could not mount %1 partition</source>
        <translation>Desculpe, não foi possível montar a partição %1</translation>
    </message>
    <message>
        <location filename="../mainwindow.cpp" line="734"/>
        <source>Could not create a temporary folder</source>
        <translation>Não foi possível criar uma pasta temporária</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <location filename="../about.cpp" line="52"/>
        <source>License</source>
        <translation>Licença</translation>
    </message>
    <message>
        <location filename="../about.cpp" line="53"/>
        <location filename="../about.cpp" line="63"/>
        <source>Changelog</source>
        <translation>Relatório de alterações</translation>
    </message>
    <message>
        <location filename="../about.cpp" line="54"/>
        <source>Cancel</source>
        <translation>Cancelar</translation>
    </message>
    <message>
        <location filename="../about.cpp" line="73"/>
        <source>&amp;Close</source>
        <translation>&amp;Fechar</translation>
    </message>
    <message>
        <location filename="../main.cpp" line="53"/>
        <source>MX Boot Repair</source>
        <translation>Reparador de Inicialização do MX</translation>
    </message>
    <message>
        <location filename="../main.cpp" line="86"/>
        <source>Error</source>
        <translation>Ocorreu um Erro</translation>
    </message>
    <message>
        <location filename="../main.cpp" line="87"/>
        <source>You must run this program with admin access.</source>
        <translation>Você tem que executar este programa com o usuário administrador.</translation>
    </message>
</context>
</TS>
