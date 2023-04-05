SELECT pkm.name
FROM Pokemon AS pkm
WHERE pkm.id NOT IN (SELECT DISTINCT pid FROM CatchedPokemon)
ORDER BY pkm.name
;