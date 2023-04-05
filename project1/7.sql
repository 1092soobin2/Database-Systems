CREATE VIEW Origin_MaxLevel AS
SELECT tr.hometown, MAX(cp.level) AS maxLevel
FROM CatchedPokemon AS cp
	JOIN Trainer AS tr ON tr.id = cp.owner_id
GROUP BY tr.hometown
;


SELECT cp.nickname
FROM CatchedPokemon cp
	JOIN Trainer tr ON tr.id = cp.owner_id
WHERE EXISTS (SELECT *
              FROM Origin_MaxLevel om
              WHERE cp.level = om.maxLevel
              	AND tr.hometown = om.hometown)
ORDER BY cp.nickname
;