# Convertit un fichier binaire en liste d'octets C, à inclure dans un tableau.
# Usage : cmake -DINPUT=fichier -DOUTPUT=fichier.inc -P embed.cmake
file(READ ${INPUT} hex HEX)
string(REGEX REPLACE "(..)" "0x\\1," bytes "${hex}")
file(WRITE ${OUTPUT} "{${bytes}}\n")
