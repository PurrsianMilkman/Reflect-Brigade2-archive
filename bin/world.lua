local bool development = false;
function Initialize()
	-- doors
	AnimationLoad("door0", "data/door/door.dae");
	AnimationLoad("door1", "data/door/door.dae");
	AnimationLoad("door2", "data/door/door.dae");
	AnimationLoad("door3", "data/door/door.dae");
	AnimationLoad("door4", "data/door/door.dae");
	-- buttons
	AnimationLoad("button0", "data/button/button_big.dae");
	AnimationLoad("button1", "data/button/button_big.dae");
	AnimationLoad("button2", "data/button/button_big.dae");
	AnimationLoad("button3", "data/button/button_big.dae");
	-- fans
	AnimationLoad("fan0", "data/fan/fan.dae");
	-- make fans spin
	AnimationSetTarget("fan0", 1000000000);
	-- goto area 1
	SetArea(1);
end
local int area = 0;
function SetArea(_Area)
	if not (area == _Area)then
		area = _Area;
		ActivateArea(area);
		-- doors
		AnimationSetVisible("door0", false);
		AnimationSetVisible("door1", false);
		AnimationSetVisible("door2", false);
		AnimationSetVisible("door3", false);
		AnimationSetVisible("door4", false);
		-- buttons
		AnimationSetVisible("button0", false);
		AnimationSetVisible("button1", false);
		AnimationSetVisible("button2", false);
		AnimationSetVisible("button3", false);
		-- fans
		AnimationSetVisible("fan0", false);
		-- cubes
		EmoCubeSetVisible("cube0", false);
		EmoCubeSetVisible("cube1", false);
		EmoCubeSetVisible("cube2", false);
		EmoCubeSetVisible("cube3", false);
		EmoCubeSetVisible("cube4", false);
		EmoCubeSetVisible("cube5", false);
		EmoCubeSetVisible("cube6", false);
		EmoCubeSetVisible("cube7", false);
		EmoCubeSetVisible("cube8", false);
		EmoCubeSetVisible("cube9", false);
		-- marbles
		EmoCubeSetVisible("marble0", false);
		EmoCubeSetVisible("marble1", false);
		EmoCubeSetVisible("marble2", false);
		EmoCubeSetVisible("marble3", false);
		EmoCubeSetVisible("marble4", false);
		EmoCubeSetVisible("marble5", false);
		EmoCubeSetVisible("marble6", false);
		EmoCubeSetVisible("marble7", false);
		-- lamps
		LampSetVisible("lamp0", false);
		LampSetVisible("lamp1", false);
		LampSetVisible("lamp2", false);
		
		-- 1 = intro
		-- 2 = two cubes puzzle
		-- 3 = hall
		-- 4 = gravity 1
		-- 5 = hall
		
		if area == 1 or area == 2 then
			EmoCubeSetVisible("cube0", true);
			EmoCubeSetVisible("cube1", true);
			AnimationSetVisible("fan0", true);
			AnimationSetVisible("door0", true);
		end
		
		if area == 2 or area == 3 then
			AnimationSetVisible("door1", true);
			AnimationSetVisible("button0", true);
			AnimationSetVisible("button1", true);
		end
		
		if area == 3 or area ==4 then
			AnimationSetVisible("door2", true);
		end
		
		if area == 3 or area == 4 or area ==5 then 
			AnimationSetVisible("door3", true);
		end
		
		if area == 4 then
			LampSetVisible("lamp0", true);
			LampSetVisible("lamp1", true);
			LampSetVisible("lamp2", true);
		
			AnimationSetVisible("door2", true);
			
			AnimationSetVisible("button1", true);
			AnimationSetVisible("button2", true);
			AnimationSetVisible("fan0", true);
			EmoCubeSetVisible("cube2", true);
		end
		
		if area == 6 or area == 7 then
			AnimationSetVisible("door4", true);
			AnimationSetVisible("button3", true);
			
			EmoCubeSetVisible("marble0", true);
			EmoCubeSetVisible("marble1", true);
			EmoCubeSetVisible("marble2", true);
			EmoCubeSetVisible("marble3", true);
			EmoCubeSetVisible("marble4", true);
			EmoCubeSetVisible("marble5", true);
			EmoCubeSetVisible("marble6", true);
			EmoCubeSetVisible("marble7", true);
		end
		
		if area == 7 then
			
			EmoCubeSetVisible("cube3", true);
			EmoCubeSetVisible("cube4", true);
			EmoCubeSetVisible("cube5", true);
			EmoCubeSetVisible("cube6", true);
			EmoCubeSetVisible("cube7", true);
			EmoCubeSetVisible("cube8", true);
			EmoCubeSetVisible("cube9", true);
			
		end
	end
end

function GetArea()
	return area;
end

local bool button0 = false;
local bool button1 = false;
local bool enteredarea4 = false;

function ButtonSet(_name, _pressed)
	if _pressed then
		AnimationSetTarget(_name, 1);
	else
		AnimationSetTarget(_name, 0);
	end
end

function DoorSet(_name, _state)
	if _state then
		AnimationSetTarget(_name, 1);
	else
		AnimationSetTarget(_name, 0);
	end
end

function HandleProximity(_name, _players, _cubes)
	message(_name);
	-- door0
	if _name == "door0" then
		DoorSet("door0", not (_players == 0));
		if not (_players == 0) then
			SetArea(2);
			message("area2");
		end
	end
	
	-- button0
	if _name == "button0" then
		button0 = not (_players == 0 and _cubes == 0);
		ButtonSet("button0", button0);
	end
	
	-- button1
	if _name == "button1" then
		button1 = not (_players == 0 and _cubes == 0);
		ButtonSet("button1", button1);
	end
	
	-- button2
	if _name == "button2" then
		ButtonSet("button2", not (_players == 0 and _cubes == 0));
		DoorSet("door3", not (_players == 0 and _cubes == 0));
	end
	if development then
		button0 = true;
		button1 = true;
	end
	-- door1
	if button0 and button1 and GetArea() == 2 then
		DoorSet("door1", true);
	else
		DoorSet("door1", false);
	end	
	
	if _name == "door1" then
		SetArea(3);
		DoorSet("door1", false);
	end
	
	-- elevator0
	if _name == "elevator0" then
		if _players == 0 then
			AnimationSetTarget("elevator0", 0);
		else
			AnimationSetTarget("elevator0", 1);
		end
	end
	
	-- gravity0
	if _name == "gravity0" then
		if not (players == 0) then
			SetGravity(0, -7, 7);
		end
	end
	
	-- gravity1
	if _name == "gravity1" then
		if not (players == 0) then
			SetGravity(0, -10, 0);
		end
	end
	
	-- door2
	if _name == "door2" and (not enteredarea4) then
		DoorSet("door2", not (_players == 0));
	end
	if _name == "closedoor2" then
		if not (_players == 0) then
			enteredarea4 = true;
			SetArea(4);
			message("area4");
			DoorSet("door2", false);
		end
	end
	
	
	
	if _name == "closedoor3" then
		if not (_players == 0) then
			SetArea(5);
			message("area5");
			DoorSet("door3", false);
		end
	end
	
	-- gravity2
	if _name == "gravity2" then
		if not (players == 0) then
			SetGravity(0, 10, -1);
		end
	end
	
	-- gravity3
	if _name == "gravity3" then
		if not (players == 0) then
			SetGravity(0, -10, 0);
		end
	end
	
	if _name == "area6" then
		if not (_players == 0) then
			SetArea(6);
			message("area6");
			
		end
	end
	
	if _name == "button3" then
		ButtonSet("button3", not (_players == 0 and _cubes == 0));
		DoorSet("door4", not (_players == 0 and _cubes == 0));
	end
	
		
	if _name == "credits" then
		if not (_players == 0) then
			SetArea(7);
			message("area7");
			
		end
	end
end
