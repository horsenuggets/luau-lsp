-- Lune global type definitions
-- These globals are provided by the Lune runtime (horsenuggets/lune
-- fork) and mimic a subset of Roblox globals.

-- Datatype classes

declare class Color3
	B: number
	G: number
	R: number
	b: number
	function Lerp(self, color: Color3, alpha: number): Color3
	function ToHSV(self): (number, number, number)
	function ToHex(self): string
	function lerp(self, color: Color3, alpha: number): Color3
	g: number
	r: number
end

declare class Vector2
	Magnitude: number
	Unit: Vector2
	X: number
	Y: number
	function Abs(self): Vector2
	function Angle(self, other: Vector2, isSigned: boolean): number
	function Ceil(self): Vector2
	function Cross(self, other: Vector2): number
	function Dot(self, v: Vector2): number
	function Floor(self): Vector2
	function FuzzyEq(self, other: Vector2, epsilon: number?): boolean
	function Lerp(self, v: Vector2, alpha: number): Vector2
	function Max(self, ...: (Vector2)): Vector2
	function Min(self, ...: (Vector2)): Vector2
	function Sign(self): Vector2
	function __add(self, other: Vector2): Vector2
	function __div(self, other: Vector2 | number): Vector2
	function __mul(self, other: Vector2 | number): Vector2
	function __sub(self, other: Vector2): Vector2
	function __unm(self): Vector2
	function lerp(self, v: Vector2, alpha: number): Vector2
	magnitude: number
	unit: Vector2
	x: number
	y: number
end

declare class Vector3
	Magnitude: number
	Unit: Vector3
	X: number
	Y: number
	Z: number
	function Abs(self): Vector3
	function Angle(self, other: Vector3, axis: Vector3?): number
	function Ceil(self): Vector3
	function Cross(self, other: Vector3): Vector3
	function Dot(self, other: Vector3): number
	function Floor(self): Vector3
	function FuzzyEq(self, other: Vector3, epsilon: number?): boolean
	function Lerp(self, goal: Vector3, alpha: number): Vector3
	function Max(self, ...: (Vector3)): Vector3
	function Min(self, ...: (Vector3)): Vector3
	function Sign(self): Vector3
	function __add(self, other: Vector3): Vector3
	function __div(self, other: Vector3 | number): Vector3
	function __mul(self, other: Vector3 | number): Vector3
	function __sub(self, other: Vector3): Vector3
	function __unm(self): Vector3
	function lerp(self, goal: Vector3, alpha: number): Vector3
	magnitude: number
	unit: Vector3
	x: number
	y: number
	z: number
end

declare class CFrame
	LookVector: Vector3
	Position: Vector3
	RightVector: Vector3
	Rotation: CFrame
	UpVector: Vector3
	X: number
	XVector: Vector3
	Y: number
	YVector: Vector3
	Z: number
	ZVector: Vector3
	function FuzzyEq(self, other: CFrame, epsilon: number?): boolean
	function GetComponents(self): (number, number, number, number, number, number, number, number, number, number, number, number)
	function Inverse(self): CFrame
	function Lerp(self, goal: CFrame, alpha: number): CFrame
	function Orthonormalize(self): CFrame
	function PointToObjectSpace(self, v3: Vector3): Vector3
	function PointToWorldSpace(self, v3: Vector3): Vector3
	function ToAxisAngle(self): (Vector3, number)
	function ToEulerAnglesXYZ(self): (number, number, number)
	function ToEulerAnglesYXZ(self): (number, number, number)
	function ToObjectSpace(self, cf: CFrame): CFrame
	function ToOrientation(self): (number, number, number)
	function ToWorldSpace(self, cf: CFrame): CFrame
	function VectorToObjectSpace(self, v3: Vector3): Vector3
	function VectorToWorldSpace(self, v3: Vector3): Vector3
	function __add(self, other: Vector3): CFrame
	function __mul(self, other: CFrame): CFrame
	function __mul(self, other: Vector3): Vector3
	function __sub(self, other: Vector3): CFrame
	function components(self): (number, number, number, number, number, number, number, number, number, number, number, number)
	function inverse(self): CFrame
	function lerp(self, goal: CFrame, alpha: number): CFrame
	lookVector: Vector3
	p: Vector3
	rightVector: Vector3
	upVector: Vector3
	x: number
	y: number
	z: number
end

declare class NumberRange
	Max: number
	Min: number
end

-- Datatype constructors

declare Color3: {
	fromRGB: ((red: number?, green: number?, blue: number?) -> Color3),
	fromHSV: ((hue: number, saturation: number, value: number) -> Color3),
	toHSV: ((color: Color3) -> (number, number, number)),
	new: ((red: number?, green: number?, blue: number?) -> Color3),
	fromHex: ((hex: string) -> Color3),
}

declare Vector2: {
	zero: Vector2,
	one: Vector2,
	xAxis: Vector2,
	yAxis: Vector2,
	new: ((x: number?, y: number?) -> Vector2),
	min: ((...Vector2) -> Vector2),
	max: ((...Vector2) -> Vector2),
}

declare Vector3: {
	zero: Vector3,
	one: Vector3,
	xAxis: Vector3,
	yAxis: Vector3,
	zAxis: Vector3,
	fromNormalId: ((normal: any) -> Vector3),
	fromAxis: ((axis: any) -> Vector3),
	new: ((x: number?, y: number?, z: number?) -> Vector3),
	min: ((...Vector3) -> Vector3),
	max: ((...Vector3) -> Vector3),
}

declare CFrame: {
	identity: CFrame,
	fromEulerAnglesYXZ: ((rx: number, ry: number, rz: number) -> CFrame),
	fromEulerAngles: ((rx: number, ry: number, rz: number) -> CFrame),
	Angles: ((rx: number, ry: number, rz: number) -> CFrame),
	fromMatrix: ((pos: Vector3, vX: Vector3, vY: Vector3, vZ: Vector3?) -> CFrame),
	fromAxisAngle: ((v: Vector3, r: number) -> CFrame),
	fromOrientation: ((rx: number, ry: number, rz: number) -> CFrame),
	fromEulerAnglesXYZ: ((rx: number, ry: number, rz: number) -> CFrame),
	lookAt: ((at: Vector3, target: Vector3, up: Vector3?) -> CFrame),
	lookAlong: ((at: Vector3, direction: Vector3, up: Vector3?) -> CFrame),
	new: (() -> CFrame) & ((pos: Vector3) -> CFrame) & ((pos: Vector3, lookAt: Vector3) -> CFrame) & ((x: number, y: number, z: number) -> CFrame) & ((x: number, y: number, z: number, qX: number, qY: number, qZ: number, qW: number) -> CFrame),
}

declare NumberRange: {
	new: ((value: number) -> NumberRange) & ((min: number, max: number) -> NumberRange),
}

-- Lune-specific globals

declare script: {
	Name: string,
	Parent: any,
	GetFullName: (self: any) -> string,
	[string]: any,
}

declare executable: string?

declare task: {
	cancel: (thread: thread) -> (),
	defer: <A..., R...>(threadOrFunction: thread | (A...) -> R..., A...) -> thread,
	delay: <A..., R...>(duration: number?, threadOrFunction: thread | (A...) -> R..., A...) -> thread,
	spawn: <A..., R...>(threadOrFunction: thread | (A...) -> R..., A...) -> thread,
	wait: (duration: number?) -> number,
}
