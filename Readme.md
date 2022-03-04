# hinfosvc

Jednoduchý minimalistický HTTP server poskytujúci rôzne informácie o systéme. 

Autor: Matej Matuška

## Zostavenie

Program je možné preložiť pomocou priloženého súboru Makefile pomocou píkazu `make`

## Použitie
Server je možné pustiť v príkazovom riadku príkazom:
`hinfosvc <číslo portu>`  
<číslo portu> je číslo portu, na ktorom má server prijímať žiadosti.

Server je možné ukončiť klávesovou skratkou Ctrl-C alebo klasickými UNIXovými príkazmi. Napríklad:  
`killall hinfosvc` alebo `kill <pid procesu hinfosvc>`

## Žiadosti
Server spracováva 3 tipy žiadostí, ktoré je možné posielať napr. prostredníctvom webového prehliadača alebo aj príkazmi `wget` a `curl`: 

Príklady použitia pomocou `curl`:

- Meno hosťa `/hostname`
- Meno procesora `/cpu-name`
- Záťaž procesora `load`