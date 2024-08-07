-- Delete everything
DELETE FROM Technology_PrereqTechs;

-- Then add them all back!
INSERT INTO Technology_PrereqTechs
	(TechType, PrereqTech)
VALUES
	('TECH_FUTURE_TECH', 'TECH_NANOTECHNOLOGY'),
	('TECH_FUTURE_TECH', 'TECH_PARTICLE_PHYSICS'),
	('TECH_FUTURE_TECH', 'TECH_NUCLEAR_FUSION'),
	('TECH_FUTURE_TECH', 'TECH_GLOBALIZATION'),
	('TECH_NANOTECHNOLOGY', 'TECH_ROBOTICS'),
	('TECH_NANOTECHNOLOGY', 'TECH_INTERNET'),
	('TECH_PARTICLE_PHYSICS', 'TECH_ROBOTICS'),
	('TECH_PARTICLE_PHYSICS', 'TECH_INTERNET'),
	('TECH_PARTICLE_PHYSICS', 'TECH_LASERS'),
	('TECH_GLOBALIZATION', 'TECH_INTERNET'),
	('TECH_GLOBALIZATION', 'TECH_LASERS'),
	('TECH_GLOBALIZATION', 'TECH_STEALTH'),
	('TECH_NUCLEAR_FUSION', 'TECH_LASERS'),
	('TECH_NUCLEAR_FUSION', 'TECH_STEALTH'),
	('TECH_ROBOTICS', 'TECH_ECOLOGY'),
	('TECH_ROBOTICS', 'TECH_TELECOM'),
	('TECH_INTERNET', 'TECH_TELECOM'),
	('TECH_INTERNET', 'TECH_SATELLITES'),
	('TECH_LASERS', 'TECH_SATELLITES'),
	('TECH_LASERS', 'TECH_ADVANCED_BALLISTICS'),
	('TECH_STEALTH', 'TECH_ADVANCED_BALLISTICS'),
	('TECH_STEALTH', 'TECH_MOBILE_TACTICS'),
	('TECH_ECOLOGY', 'TECH_ELECTRONICS'),
	('TECH_TELECOM', 'TECH_ELECTRONICS'),
	('TECH_TELECOM', 'TECH_COMPUTERS'),
	('TECH_SATELLITES', 'TECH_COMPUTERS'),
	('TECH_SATELLITES', 'TECH_NUCLEAR_FISSION'),
	('TECH_ADVANCED_BALLISTICS', 'TECH_NUCLEAR_FISSION'),
	('TECH_ADVANCED_BALLISTICS', 'TECH_RADAR'),
	('TECH_MOBILE_TACTICS', 'TECH_RADAR'),
	('TECH_ELECTRONICS', 'TECH_PENICILIN'),
	('TECH_ELECTRONICS', 'TECH_REFRIGERATION'),
	('TECH_COMPUTERS', 'TECH_REFRIGERATION'),
	('TECH_COMPUTERS', 'TECH_ATOMIC_THEORY'),
	('TECH_NUCLEAR_FISSION', 'TECH_ATOMIC_THEORY'),
	('TECH_NUCLEAR_FISSION', 'TECH_ROCKETRY'),
	('TECH_RADAR', 'TECH_ROCKETRY'),
	('TECH_RADAR', 'TECH_COMBINED_ARMS'),
	('TECH_PENICILIN', 'TECH_PLASTIC'),
	('TECH_REFRIGERATION', 'TECH_PLASTIC'),
	('TECH_REFRIGERATION', 'TECH_RADIO'),
	('TECH_ATOMIC_THEORY', 'TECH_RADIO'),
	('TECH_ATOMIC_THEORY', 'TECH_FLIGHT'),
	('TECH_ROCKETRY', 'TECH_FLIGHT'),
	('TECH_ROCKETRY', 'TECH_BALLISTICS'),
	('TECH_COMBINED_ARMS', 'TECH_BALLISTICS'),
	('TECH_PLASTIC', 'TECH_BIOLOGY'),
	('TECH_PLASTIC', 'TECH_ELECTRICITY'),
	('TECH_RADIO', 'TECH_ELECTRICITY'),
	('TECH_RADIO', 'TECH_CORPORATIONS'),
	('TECH_FLIGHT', 'TECH_CORPORATIONS'),
	('TECH_FLIGHT', 'TECH_REPLACEABLE_PARTS'),
	('TECH_BALLISTICS', 'TECH_REPLACEABLE_PARTS'),
	('TECH_BALLISTICS', 'TECH_COMBUSTION'),
	('TECH_BIOLOGY', 'TECH_ARCHAEOLOGY'),
	('TECH_BIOLOGY', 'TECH_FERTILIZER'),
	('TECH_ELECTRICITY', 'TECH_ARCHAEOLOGY'),
	('TECH_ELECTRICITY', 'TECH_FERTILIZER'),
	('TECH_ELECTRICITY', 'TECH_INDUSTRIALIZATION'),
	('TECH_CORPORATIONS', 'TECH_FERTILIZER'),
	('TECH_CORPORATIONS', 'TECH_INDUSTRIALIZATION'),
	('TECH_CORPORATIONS', 'TECH_DYNAMITE'),
	('TECH_REPLACEABLE_PARTS', 'TECH_INDUSTRIALIZATION'),
	('TECH_REPLACEABLE_PARTS', 'TECH_DYNAMITE'),
	('TECH_REPLACEABLE_PARTS', 'TECH_MILITARY_SCIENCE'),
	('TECH_COMBUSTION', 'TECH_DYNAMITE'),
	('TECH_COMBUSTION', 'TECH_MILITARY_SCIENCE'),
	('TECH_ARCHAEOLOGY', 'TECH_SCIENTIFIC_THEORY'),
	('TECH_FERTILIZER', 'TECH_SCIENTIFIC_THEORY'),
	('TECH_FERTILIZER', 'TECH_RAILROAD'),
	('TECH_INDUSTRIALIZATION', 'TECH_RAILROAD'),
	('TECH_INDUSTRIALIZATION', 'TECH_STEAM_POWER'),
	('TECH_DYNAMITE', 'TECH_STEAM_POWER'),
	('TECH_DYNAMITE', 'TECH_RIFLING'),
	('TECH_MILITARY_SCIENCE', 'TECH_RIFLING'),
	('TECH_SCIENTIFIC_THEORY', 'TECH_ARCHITECTURE'),
	('TECH_SCIENTIFIC_THEORY', 'TECH_ECONOMICS'),
	('TECH_RAILROAD', 'TECH_ECONOMICS'),
	('TECH_RAILROAD', 'TECH_ACOUSTICS'),
	('TECH_STEAM_POWER', 'TECH_ACOUSTICS'),
	('TECH_STEAM_POWER', 'TECH_NAVIGATION'),
	('TECH_RIFLING', 'TECH_NAVIGATION'),
	('TECH_RIFLING', 'TECH_METALLURGY'),
	('TECH_ARCHITECTURE', 'TECH_BANKING'),
	('TECH_ARCHITECTURE', 'TECH_PRINTING_PRESS'),
	('TECH_ECONOMICS', 'TECH_BANKING'),
	('TECH_ECONOMICS', 'TECH_PRINTING_PRESS'),
	('TECH_ECONOMICS', 'TECH_ASTRONOMY'),
	('TECH_ACOUSTICS', 'TECH_PRINTING_PRESS'),
	('TECH_ACOUSTICS', 'TECH_ASTRONOMY'),
	('TECH_ACOUSTICS', 'TECH_GUNPOWDER'),
	('TECH_NAVIGATION', 'TECH_ASTRONOMY'),
	('TECH_NAVIGATION', 'TECH_GUNPOWDER'),
	('TECH_NAVIGATION', 'TECH_CHEMISTRY'),
	('TECH_METALLURGY', 'TECH_GUNPOWDER'),
	('TECH_METALLURGY', 'TECH_CHEMISTRY'),
	('TECH_BANKING', 'TECH_CIVIL_SERVICE'),
	('TECH_PRINTING_PRESS', 'TECH_CIVIL_SERVICE'),
	('TECH_PRINTING_PRESS', 'TECH_GUILDS'),
	('TECH_ASTRONOMY', 'TECH_GUILDS'),
	('TECH_ASTRONOMY', 'TECH_COMPASS'),
	('TECH_GUNPOWDER', 'TECH_COMPASS'),
	('TECH_GUNPOWDER', 'TECH_MACHINERY'),
	('TECH_CHEMISTRY', 'TECH_MACHINERY'),
	('TECH_CIVIL_SERVICE', 'TECH_EDUCATION'),
	('TECH_CIVIL_SERVICE', 'TECH_THEOLOGY'),
	('TECH_GUILDS', 'TECH_THEOLOGY'),
	('TECH_GUILDS', 'TECH_CHIVALRY'),
	('TECH_COMPASS', 'TECH_CHIVALRY'),
	('TECH_COMPASS', 'TECH_PHYSICS'),
	('TECH_MACHINERY', 'TECH_PHYSICS'),
	('TECH_MACHINERY', 'TECH_STEEL'),
	('TECH_EDUCATION', 'TECH_DRAMA'),
	('TECH_EDUCATION', 'TECH_PHILOSOPHY'),
	('TECH_THEOLOGY', 'TECH_DRAMA'),
	('TECH_THEOLOGY', 'TECH_PHILOSOPHY'),
	('TECH_THEOLOGY', 'TECH_CURRENCY'),
	('TECH_CHIVALRY', 'TECH_PHILOSOPHY'),
	('TECH_CHIVALRY', 'TECH_CURRENCY'),
	('TECH_CHIVALRY', 'TECH_ENGINEERING'),
	('TECH_PHYSICS', 'TECH_CURRENCY'),
	('TECH_PHYSICS', 'TECH_ENGINEERING'),
	('TECH_PHYSICS', 'TECH_METAL_CASTING'),
	('TECH_STEEL', 'TECH_ENGINEERING'),
	('TECH_STEEL', 'TECH_METAL_CASTING'),
	('TECH_DRAMA', 'TECH_OPTICS'),
	('TECH_DRAMA', 'TECH_WRITING'),
	('TECH_PHILOSOPHY', 'TECH_OPTICS'),
	('TECH_PHILOSOPHY', 'TECH_WRITING'),
	('TECH_PHILOSOPHY', 'TECH_MATHEMATICS'),
	('TECH_CURRENCY', 'TECH_WRITING'),
	('TECH_CURRENCY', 'TECH_MATHEMATICS'),
	('TECH_CURRENCY', 'TECH_CONSTRUCTION'),
	('TECH_ENGINEERING', 'TECH_MATHEMATICS'),
	('TECH_ENGINEERING', 'TECH_CONSTRUCTION'),
	('TECH_ENGINEERING', 'TECH_IRON_WORKING'),
	('TECH_METAL_CASTING', 'TECH_CONSTRUCTION'),
	('TECH_METAL_CASTING', 'TECH_IRON_WORKING'),
	('TECH_OPTICS', 'TECH_SAILING'),
	('TECH_OPTICS', 'TECH_HORSEBACK_RIDING'),
	('TECH_WRITING', 'TECH_HORSEBACK_RIDING'),
	('TECH_WRITING', 'TECH_CALENDAR'),
	('TECH_MATHEMATICS', 'TECH_CALENDAR'),
	('TECH_MATHEMATICS', 'TECH_MASONRY'),
	('TECH_CONSTRUCTION', 'TECH_MASONRY'),
	('TECH_CONSTRUCTION', 'TECH_ARCHERY'),
	('TECH_IRON_WORKING', 'TECH_ARCHERY'),
	('TECH_IRON_WORKING', 'TECH_BRONZE_WORKING'),
	('TECH_SAILING', 'TECH_POTTERY'),
	('TECH_HORSEBACK_RIDING', 'TECH_POTTERY'),
	('TECH_HORSEBACK_RIDING', 'TECH_TRAPPING'),
	('TECH_CALENDAR', 'TECH_TRAPPING'),
	('TECH_CALENDAR', 'TECH_THE_WHEEL'),
	('TECH_MASONRY', 'TECH_THE_WHEEL'),
	('TECH_MASONRY', 'TECH_ANIMAL_HUSBANDRY'),
	('TECH_ARCHERY', 'TECH_ANIMAL_HUSBANDRY'),
	('TECH_ARCHERY', 'TECH_MINING'),
	('TECH_BRONZE_WORKING', 'TECH_MINING'),
	('TECH_POTTERY', 'TECH_AGRICULTURE'),
	('TECH_TRAPPING', 'TECH_AGRICULTURE'),
	('TECH_THE_WHEEL', 'TECH_AGRICULTURE'),
	('TECH_ANIMAL_HUSBANDRY', 'TECH_AGRICULTURE'),
	('TECH_MINING', 'TECH_AGRICULTURE');

-- Coordinates on the Tech Tree
CREATE TEMP TABLE TechCoordinates (
	Tech TEXT,
	X INTEGER,
	Y INTEGER
);

INSERT INTO TechCoordinates
VALUES
	-- Start
	('TECH_AGRICULTURE', 0, 5),
	-- Ancient T1
	('TECH_POTTERY', 1, 1),
	('TECH_TRAPPING', 1, 3),
	('TECH_THE_WHEEL', 1, 5),
	('TECH_ANIMAL_HUSBANDRY', 1, 7),
	('TECH_MINING', 1, 9),
	-- Ancient T2
	('TECH_SAILING', 2, 1),
	('TECH_HORSEBACK_RIDING', 2, 2),
	('TECH_CALENDAR', 2, 4),
	('TECH_MASONRY', 2, 6),
	('TECH_ARCHERY', 2, 8),
	('TECH_BRONZE_WORKING', 2, 9),
	-- Classical T1
	('TECH_OPTICS', 3, 1),
	('TECH_WRITING', 3, 3),
	('TECH_MATHEMATICS', 3, 5),
	('TECH_CONSTRUCTION', 3, 7),
	('TECH_IRON_WORKING', 3, 9),
	-- Classical T2
	('TECH_DRAMA', 4, 1),
	('TECH_PHILOSOPHY', 4, 3),
	('TECH_CURRENCY', 4, 5),
	('TECH_ENGINEERING', 4, 7),
	('TECH_METAL_CASTING', 4, 9),
	-- Medieval T1
	('TECH_EDUCATION', 5, 1),
	('TECH_THEOLOGY', 5, 3),
	('TECH_CHIVALRY', 5, 5),
	('TECH_PHYSICS', 5, 7),
	('TECH_STEEL', 5, 9),
	-- Medieval T2
	('TECH_CIVIL_SERVICE', 6, 2),
	('TECH_GUILDS', 6, 4),
	('TECH_COMPASS', 6, 6),
	('TECH_MACHINERY', 6, 8),
	-- Renaissance T1
	('TECH_BANKING', 7, 1),
	('TECH_PRINTING_PRESS', 7, 3),
	('TECH_ASTRONOMY', 7, 5),
	('TECH_GUNPOWDER', 7, 7),
	('TECH_CHEMISTRY', 7, 9),
	-- Renaissance T2
	('TECH_ARCHITECTURE', 8, 1),
	('TECH_ECONOMICS', 8, 3),
	('TECH_ACOUSTICS', 8, 5),
	('TECH_NAVIGATION', 8, 7),
	('TECH_METALLURGY', 8, 9),
	-- Industrial T1
	('TECH_SCIENTIFIC_THEORY', 9, 2),
	('TECH_RAILROAD', 9, 4),
	('TECH_STEAM_POWER', 9, 6),
	('TECH_RIFLING', 9, 8),
	-- Industrial T2
	('TECH_ARCHAEOLOGY', 10, 1),
	('TECH_FERTILIZER', 10, 3),
	('TECH_INDUSTRIALIZATION', 10, 5),
	('TECH_DYNAMITE', 10, 7),
	('TECH_MILITARY_SCIENCE', 10, 9),
	-- Modern T1
	('TECH_BIOLOGY', 11, 1),
	('TECH_ELECTRICITY', 11, 3),
	('TECH_CORPORATIONS', 11, 5),
	('TECH_REPLACEABLE_PARTS', 11, 7),
	('TECH_COMBUSTION', 11, 9),
	-- Modern T2
	('TECH_PLASTIC', 12, 2),
	('TECH_RADIO', 12, 4),
	('TECH_FLIGHT', 12, 6),
	('TECH_BALLISTICS', 12, 8),
	-- Atomic T1
	('TECH_PENICILIN', 13, 1),
	('TECH_REFRIGERATION', 13, 3),
	('TECH_ATOMIC_THEORY', 13, 5),
	('TECH_ROCKETRY', 13, 7),
	('TECH_COMBINED_ARMS', 13, 9),
	-- Atomic T2
	('TECH_ELECTRONICS', 14, 2),
	('TECH_COMPUTERS', 14, 4),
	('TECH_NUCLEAR_FISSION', 14, 6),
	('TECH_RADAR', 14, 8),
	-- Information T1
	('TECH_ECOLOGY', 15, 1),
	('TECH_TELECOM', 15, 3),
	('TECH_SATELLITES', 15, 5),
	('TECH_ADVANCED_BALLISTICS', 15, 7),
	('TECH_MOBILE_TACTICS', 15, 9),
	-- Information T2
	('TECH_ROBOTICS', 16, 2),
	('TECH_INTERNET', 16, 4),
	('TECH_LASERS', 16, 6),
	('TECH_STEALTH', 16, 8),
	-- Information T3
	('TECH_NANOTECHNOLOGY', 17, 2),
	('TECH_PARTICLE_PHYSICS', 17, 4),
	('TECH_GLOBALIZATION', 17, 6),
	('TECH_NUCLEAR_FUSION', 17, 8),
	-- Future
	('TECH_FUTURE_TECH', 18, 5);

UPDATE Technologies
SET
	GridX = (SELECT X FROM TechCoordinates WHERE Tech = Type),
	GridY = (SELECT Y FROM TechCoordinates WHERE Tech = Type)
WHERE EXISTS (SELECT 1 FROM TechCoordinates WHERE Tech = Type);

DROP TABLE TechCoordinates;
