  create table raw_data (
    id integer primary key,
    name text,
    team integer,
    difficulty integer,
    redPoint integer,
    bluePoint integer,
    greenPoint integer,
    hitPoint integer,
    remainingTime integer
  );

  create table player_data (
    id integer primary key,
    name text,
    team text,
    difficulty text,
    score integer,
    rank text,
    virusnum integer,
    gameResult integer,
    redPoint integer,
    bluePoint integer,
    greenPoint integer,
    hitPoint integer,
    remainingTime integer
  );
