COLOR='\033[0;33m\033[40m'
NO_COLOR='\033[0m'

echo -e "${COLOR}> ls -l /tmp${NO_COLOR}"
ls -l /tmp
echo
echo -e "${COLOR}> ipcs${NO_COLOR}"
ipcs
echo -e "${COLOR}> ps -fu \`id -u\`${NO_COLOR}"
ps -fu `id -u`
