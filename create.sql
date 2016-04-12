R"=====(

CREATE TABLE Access(
  Id INTEGER PRIMARY KEY NOT NULL,
  Instruction INTEGER,
  Position INTEGER,
  Reference INTEGER,
  Type INTEGER,
  State INTEGER,
  FOREIGN KEY(Reference) REFERENCES Reference(Id)
);

CREATE TABLE Call(
  Id INTEGER PRIMARY KEY NOT NULL,
  Thread INTEGER,
  Function INTEGER,
  Instruction INTEGER,
  Start INTEGER,
  End INTEGER,
  FOREIGN KEY(Thread) REFERENCES Thread(Id),
  FOREIGN KEY(Function) REFERENCES Function(Id),
  FOREIGN KEY(Instruction) REFERENCES Instruction(Id)
);

CREATE TABLE File(
  Id INTEGER PRIMARY KEY NOT NULL,
  Path VARCHAR,
  Image INTEGER,
  FOREIGN KEY(Image) REFERENCES Image(Id)
);

CREATE TABLE Function(
  Id INTEGER PRIMARY KEY NOT NULL,
  Name VARCHAR,
  Prototype VARCHAR,
  File INTEGER,
  Line INTEGER,
  Column INTEGER,
  FOREIGN KEY(File) REFERENCES File(Id),
  CONSTRAINT UniqueFunction UNIQUE (Name, Prototype, File, Line, Column)
);

CREATE TABLE Image (
    Id INTEGER PRIMARY KEY NOT NULL,
    Name VARCHAR
);

CREATE TABLE Instruction(
  Id INTEGER PRIMARY KEY NOT NULL,
  Address INTEGER,
  Segment INTEGER,
  Type INTEGER,
  Line INTEGER,
  Column INTEGER,
  TSC INTEGER,
  FOREIGN KEY(Segment) REFERENCES Segment(Id),
  CONSTRAINT UniqueInstructionTime UNIQUE (TSC)
);

CREATE TABLE Loop(
  Id INTEGER PRIMARY KEY NOT NULL,
  Function INTEGER,
  Line INTEGER,
  Column INTEGER,
  FOREIGN KEY(Function) REFERENCES Function(Id)
);

CREATE TABLE LoopExecution(
  Id INTEGER PRIMARY KEY NOT NULL,
  Loop INTEGER,
  ParentIteration INTEGER,
  Start INTEGER,
  End INTEGER,
  FOREIGN KEY(Loop) REFERENCES Loop(Id),
  FOREIGN KEY(ParentIteration) REFERENCES LoopIteration(Id)
);

CREATE TABLE LoopIteration(
  Id INTEGER PRIMARY KEY NOT NULL,
  Execution INTEGER,
  Iteration INTEGER,
  FOREIGN KEY(Execution) REFERENCES LoopExecution(Id)
);

CREATE TABLE Member(
  Id INTEGER PRIMARY KEY NOT NULL,
  Name VARCHAR
);

CREATE TABLE Reference(
  Id INTEGER PRIMARY KEY NOT NULL,
  Size INTEGER,
  Type INTEGER,
  Name VARCHAR,
  Allocator INTEGER,
  Member INTEGER,
  FOREIGN KEY(Allocator) REFERENCES Instruction(Id),
  FOREIGN KEY(Member) REFERENCES Member(Id)
);

CREATE TABLE Segment(
  Id INTEGER PRIMARY KEY NOT NULL,
  Call INTEGER,
  Type INTEGER,
  LoopIteration INTEGER,
  FOREIGN KEY(Call) REFERENCES Call(Id),
  FOREIGN KEY(LoopIteration) REFERENCES LoopIteration(Id)
);

CREATE TABLE SourceLocation(
  Id INTEGER PRIMARY KEY NOT NULL,
  Function INTEGER,
  Line INTEGER,
  Column INTEGER,
  FOREIGN KEY(Function) REFERENCES File(Id),
  CONSTRAINT UniqueSourceLocation UNIQUE (Function, Line, Column)
);

CREATE TABLE Tag (
  Id INTEGER PRIMARY KEY NOT NULL,
  Name VARCHAR,
  Type INTEGER
);

CREATE TABLE TagHit(
  Id INTEGER PRIMARY KEY NOT NULL,
  Address INTEGER,
  TSC INTEGER,
  TagInstruction INTEGER,
  Thread INTEGER,
  FOREIGN KEY(Thread) REFERENCES Thread(Id),
  FOREIGN KEY(TagInstruction) REFERENCES TagInstruction(Id),
  CONSTRAINT UniqueTagHitTime UNIQUE (TSC)
);

CREATE TABLE TagInstance(
  Id INTEGER PRIMARY KEY NOT NULL,
  Tag INTEGER,
  Start INTEGER,
  End INTEGER,
  Thread INTEGER,
  Counter INTEGER,
  FOREIGN KEY(Thread) REFERENCES Thread(Id),
  FOREIGN KEY(Tag) REFERENCES Tag(Id)
);

CREATE TABLE TagInstruction(
  Id INTEGER PRIMARY KEY NOT NULL,
  Tag INTEGER,
  Location INTEGER,
  Type INTEGER,
  FOREIGN KEY(Location) REFERENCES SourceLocation(Id),
  FOREIGN KEY(Tag) REFERENCES Tag(Id),
  CONSTRAINT UniqueTagInstruction UNIQUE (Tag, Location, Type)
);

CREATE TABLE Thread(
  Id INTEGER PRIMARY KEY NOT NULL,
  Instruction INTEGER,
  StartTime VARCHAR,
  EndTSC INTEGER,
  EndTime VARCHAR,
  FOREIGN KEY(Instruction) REFERENCES Instruction(Id)
);

)====="
