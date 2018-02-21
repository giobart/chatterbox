#!/bin/bash

if [[ $# != 2 ]]; then
    echo "usa $0 path_file_configurazione_server tempo_modifica"
    exit 1
fi

if [[ $1 == "--help" ]] || [[ $2 == "--help" ]]; then
	echo "usa $0 path_file_configurazione_server tempo_modifica"
    exit 1
fi

if ! [ -e $1 ]; then
	echo "config $1 non valido"
	exit 1
fi

myfile=$1 


#pharsing con estrazione della parole $1
function pharser() {
	local line
	local primocarattere
	#apertura file in lettura
	exec 3< $myfile
	IFS=" "
	while read -u 3 line ; do
		primocarattere=$(echo $line | head -c 1)
		if [[ $primocarattere != '#' ]]; then
			#echo $line
			array=( $line )
			if [[ ${array[0]} == $1 ]]; then
				risultato=${array[2]}
			fi 
		fi
	done
} 

pharser "DirName"

echo "Directory trovata $risultato"

cd $risultato/

if [ $2 != '0' ]; then
 echo "Eliminazione files più vecchi di $2 minuti"
 command find . -type f -mmin +$2 -delete 
 exit 0;
fi

ls $risultato



