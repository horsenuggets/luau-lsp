-- Lune global type definitions.
-- Roblox exports are shared with @lune/roblox. Only Lune's supported runtime surface
-- is described here, not the complete Roblox engine API.

-- Datatype classes.

declare extern type Color3 with
	B: number
	G: number
	R: number
	function Lerp(self, color: Color3, alpha: number): Color3
	function ToHSV(self): (number, number, number)
	function ToHex(self): string
	function __add(self, other: Color3): Color3
	function __div(self, other: Color3 | number): Color3
	function __mul(self, other: Color3 | number): Color3
	function __sub(self, other: Color3): Color3
	function __unm(self): Color3
end

declare extern type Vector2 with
	Magnitude: number
	Unit: Vector2
	X: number
	Y: number
	function Abs(self): Vector2
	function Angle(self, other: Vector2, isSigned: boolean?): number
	function Ceil(self): Vector2
	function Cross(self, other: Vector2): number
	function Dot(self, other: Vector2): number
	function Floor(self): Vector2
	function FuzzyEq(self, other: Vector2, epsilon: number?): boolean
	function Lerp(self, other: Vector2, alpha: number): Vector2
	function Max(self, ...: Vector2): Vector2
	function Min(self, ...: Vector2): Vector2
	function Sign(self): Vector2
	function __add(self, other: Vector2): Vector2
	function __div(self, other: Vector2 | number): Vector2
	function __idiv(self, other: Vector2 | number): Vector2
	function __mul(self, other: Vector2 | number): Vector2
	function __sub(self, other: Vector2): Vector2
	function __unm(self): Vector2
end

declare extern type Vector3 with
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
	function Max(self, ...: Vector3): Vector3
	function Min(self, ...: Vector3): Vector3
	function Sign(self): Vector3
	function __add(self, other: Vector3): Vector3
	function __div(self, other: Vector3 | number): Vector3
	function __idiv(self, other: Vector3 | number): Vector3
	function __mul(self, other: Vector3 | number): Vector3
	function __sub(self, other: Vector3): Vector3
	function __unm(self): Vector3
end

declare extern type CFrame with
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
	function AngleBetween(self, other: CFrame): number
	function FuzzyEq(self, other: CFrame, epsilon: number?): boolean
	function GetComponents(self): (number, number, number, number, number, number, number, number, number, number, number, number)
	function Inverse(self): CFrame
	function Lerp(self, goal: CFrame, alpha: number): CFrame
	function Orthonormalize(self): CFrame
	function PointToObjectSpace(self, ...: Vector3): ...Vector3
	function PointToWorldSpace(self, ...: Vector3): ...Vector3
	function ToAxisAngle(self): (Vector3, number)
	function ToEulerAngles(self, order: EnumItem?): (number, number, number)
	function ToEulerAnglesXYZ(self): (number, number, number)
	function ToEulerAnglesYXZ(self): (number, number, number)
	function ToObjectSpace(self, ...: CFrame): ...CFrame
	function ToOrientation(self): (number, number, number)
	function ToWorldSpace(self, ...: CFrame): ...CFrame
	function VectorToObjectSpace(self, ...: Vector3): ...Vector3
	function VectorToWorldSpace(self, ...: Vector3): ...Vector3
	function __add(self, other: Vector3): CFrame
	function __mul(self, other: CFrame): CFrame
	function __mul(self, other: Vector3): Vector3
	function __sub(self, other: Vector3): CFrame
end

declare extern type NumberRange with
	Max: number
	Min: number
end

declare extern type Axes with
	Back: boolean
	Bottom: boolean
	Front: boolean
	Left: boolean
	Right: boolean
	Top: boolean
	X: boolean
	Y: boolean
	Z: boolean
end

declare extern type BrickColor with
	B: number
	Color: Color3
	G: number
	Name: string
	Number: number
	R: number
	b: number
	g: number
	r: number
end

declare extern type ColorSequence with
	Keypoints: { ColorSequenceKeypoint }
end

declare extern type ColorSequenceKeypoint with
	Time: number
	Value: Color3
end

declare extern type Content with
	Object: Instance?
	SourceType: EnumItem
	Uri: string?
end

declare extern type Faces with
	Back: boolean
	Bottom: boolean
	Front: boolean
	Left: boolean
	Right: boolean
	Top: boolean
end

declare extern type Font with
	Bold: boolean
	Family: string
	Style: EnumItem
	Weight: EnumItem
end

declare extern type NumberSequence with
	Keypoints: { NumberSequenceKeypoint }
end

declare extern type NumberSequenceKeypoint with
	Envelope: number
	Time: number
	Value: number
end

declare extern type PhysicalProperties with
	AcousticAbsorption: number
	Density: number
	Elasticity: number
	ElasticityWeight: number
	Friction: number
	FrictionWeight: number
end

declare extern type Ray with
	Direction: Vector3
	Origin: Vector3
	Unit: Ray
	function ClosestPoint(self, point: Vector3): Vector3
	function Distance(self, point: Vector3): number
end

declare extern type Rect with
	Height: number
	Max: Vector2
	Min: Vector2
	Width: number
	function __add(self, other: Rect): Rect
	function __sub(self, other: Rect): Rect
	function __unm(self): Rect
end

declare extern type Region3 with
	CFrame: CFrame
	Size: Vector3
	function ExpandToGrid(self, resolution: number): Region3
end

declare extern type Region3int16 with
	Max: Vector3int16
	Min: Vector3int16
end

declare extern type UDim with
	Offset: number
	Scale: number
	function __add(self, other: UDim): UDim
	function __sub(self, other: UDim): UDim
	function __unm(self): UDim
end

declare extern type UDim2 with
	Height: UDim
	Width: UDim
	X: UDim
	Y: UDim
	function Lerp(self, goal: UDim2, alpha: number): UDim2
	function __add(self, other: UDim2): UDim2
	function __sub(self, other: UDim2): UDim2
	function __unm(self): UDim2
end

declare extern type UniqueId with
end

declare extern type Vector2int16 with
	X: number
	Y: number
	function __add(self, other: Vector2int16): Vector2int16
	function __div(self, other: Vector2int16 | number): Vector2int16
	function __mul(self, other: Vector2int16 | number): Vector2int16
	function __sub(self, other: Vector2int16): Vector2int16
	function __unm(self): Vector2int16
end

declare extern type Vector3int16 with
	X: number
	Y: number
	Z: number
	function __add(self, other: Vector3int16): Vector3int16
	function __div(self, other: Vector3int16 | number): Vector3int16
	function __mul(self, other: Vector3int16 | number): Vector3int16
	function __sub(self, other: Vector3int16): Vector3int16
	function __unm(self): Vector3int16
end

-- Enum names and item names come from the runtime reflection database. Indexers keep
-- their values typed without pretending this file enumerates that database.

export type Enum = {
	GetEnumItems: (self: Enum) -> { EnumItem },
	FromName: (self: Enum, name: string) -> EnumItem?,
	FromValue: (self: Enum, value: number) -> EnumItem?,
	[string]: EnumItem,
}

declare extern type EnumItem with
	EnumType: Enum
	Name: string
	Value: number
end

type Enums = {
	GetEnums: (self: Enums) -> { Enum },
	[string]: Enum,
}

declare Enum: Enums

-- Instances expose only Lune's built-in methods. Reflected class-specific properties
-- and methods installed by implementMethod/implementProperty are not inferred here.

declare extern type Instance with
	ClassName: string
	Name: string
	Parent: Instance?
	function AddTag(self, name: string): ()
	function ClearAllChildren(self): ()
	function Clone(self): Instance
	function Destroy(self): ()
	function FindFirstAncestor(self, name: string): Instance?
	function FindFirstAncestorOfClass(self, className: string): Instance?
	function FindFirstAncestorWhichIsA(self, className: string): Instance?
	function FindFirstChild(self, name: string, recursive: boolean?): Instance?
	function FindFirstChildOfClass(self, className: string, recursive: boolean?): Instance?
	function FindFirstChildWhichIsA(self, className: string, recursive: boolean?): Instance?
	function GetAttribute(self, name: string): unknown
	function GetAttributes(self): { [string]: unknown }
	function GetChildren(self): { Instance }
	function GetDebugId(self): string
	function GetDescendants(self): { Instance }
	function GetFullName(self): string
	function GetTags(self): { string }
	function HasTag(self, name: string): boolean
	function IsA(self, className: string): boolean
	function IsAncestorOf(self, descendant: Instance): boolean
	function IsDescendantOf(self, ancestor: Instance): boolean
	function QueryDescendants(self, selector: string): { Instance }
	function RemoveTag(self, name: string): ()
	function SetAttribute(self, name: string, value: unknown): ()
end

declare extern type DataModel extends Instance with
	function FindService(self, name: string): Instance?
	function GetService(self, name: string): Instance
end

-- Datatype constructors.

declare Axes: { new: (...EnumItem) -> Axes }

declare BrickColor: {
	new: ((value: number | string | Color3) -> BrickColor) & ((red: number, green: number, blue: number) -> BrickColor),
	Black: () -> BrickColor,
	Blue: () -> BrickColor,
	DarkGray: () -> BrickColor,
	Gray: () -> BrickColor,
	Green: () -> BrickColor,
	Red: () -> BrickColor,
	White: () -> BrickColor,
	Yellow: () -> BrickColor,
	palette: (index: number) -> BrickColor,
	random: () -> BrickColor,
}

declare CFrame: {
	new: (() -> CFrame)
		& ((pos: Vector3) -> CFrame)
		& ((pos: Vector3, lookAt: Vector3, up: Vector3?) -> CFrame)
		& ((x: number, y: number, z: number) -> CFrame)
		& ((x: number, y: number, z: number, qX: number, qY: number, qZ: number, qW: number) -> CFrame)
		& ((x: number, y: number, z: number, r00: number, r01: number, r02: number, r10: number, r11: number, r12: number, r20: number, r21: number, r22: number) -> CFrame),
	Angles: (rx: number, ry: number, rz: number) -> CFrame,
	fromAxisAngle: (axis: Vector3, angle: number) -> CFrame,
	fromEulerAngles: (rx: number, ry: number, rz: number, order: EnumItem?) -> CFrame,
	fromEulerAnglesXYZ: (rx: number, ry: number, rz: number) -> CFrame,
	fromEulerAnglesYXZ: (rx: number, ry: number, rz: number) -> CFrame,
	fromMatrix: (pos: Vector3, vX: Vector3, vY: Vector3, vZ: Vector3?) -> CFrame,
	fromOrientation: (rx: number, ry: number, rz: number) -> CFrame,
	fromRotationBetweenVectors: (from: Vector3, to: Vector3) -> CFrame,
	identity: CFrame,
	lookAlong: (at: Vector3, direction: Vector3, up: Vector3?) -> CFrame,
	lookAt: (at: Vector3, target: Vector3, up: Vector3?) -> CFrame,
}

declare Color3: {
	new: (red: number?, green: number?, blue: number?) -> Color3,
	fromHSV: (hue: number, saturation: number, value: number) -> Color3,
	fromHex: (hex: string) -> Color3,
	fromRGB: (red: number?, green: number?, blue: number?) -> Color3,
}

declare ColorSequence: { new: ((color: Color3, finish: Color3?) -> ColorSequence) & ((keypoints: { ColorSequenceKeypoint }) -> ColorSequence) }
declare ColorSequenceKeypoint: { new: (time: number, color: Color3) -> ColorSequenceKeypoint }
declare Content: {
	fromAssetId: (assetId: number) -> Content,
	fromObject: (object: Instance) -> Content,
	fromUri: (uri: string) -> Content,
	none: Content,
}
declare Faces: { new: (...EnumItem) -> Faces }
declare Font: {
	new: (family: string, weight: EnumItem?, style: EnumItem?) -> Font,
	fromEnum: (font: EnumItem) -> Font,
	fromId: (id: number, weight: EnumItem?, style: EnumItem?) -> Font,
	fromName: (name: string, weight: EnumItem?, style: EnumItem?) -> Font,
}
declare Instance: { new: ((className: "DataModel") -> DataModel) & ((className: string) -> Instance) }
declare NumberRange: { new: (min: number, max: number?) -> NumberRange }
declare NumberSequence: { new: ((value: number, finish: number?) -> NumberSequence) & ((keypoints: { NumberSequenceKeypoint }) -> NumberSequence) }
declare NumberSequenceKeypoint: { new: (time: number, value: number, envelope: number?) -> NumberSequenceKeypoint }
declare PhysicalProperties: {
	new: ((material: EnumItem) -> PhysicalProperties)
		& ((density: number, friction: number, elasticity: number, frictionWeight: number?, elasticityWeight: number?, acousticAbsorption: number?) -> PhysicalProperties),
}
declare Ray: { new: (origin: Vector3, direction: Vector3) -> Ray }
declare Rect: {
	new: ((min: Vector2?, max: Vector2?) -> Rect)
		& ((x0: number, y0: number?, x1: number?, y1: number?) -> Rect)
		& ((x0: nil, y0: number, x1: number?, y1: number?) -> Rect)
		& ((x0: nil, y0: nil, x1: number, y1: number?) -> Rect)
		& ((x0: nil, y0: nil, x1: nil, y1: number) -> Rect),
}
declare Region3: { new: (min: Vector3, max: Vector3) -> Region3 }
declare Region3int16: { new: (min: Vector3int16, max: Vector3int16) -> Region3int16 }
declare UDim: { new: (scale: number?, offset: number?) -> UDim }
declare UDim2: {
	new: ((x: UDim?, y: UDim?) -> UDim2)
		& ((xScale: number, xOffset: number?, yScale: number?, yOffset: number?) -> UDim2)
		& ((xScale: nil, xOffset: number, yScale: number?, yOffset: number?) -> UDim2)
		& ((xScale: nil, xOffset: nil, yScale: number, yOffset: number?) -> UDim2)
		& ((xScale: nil, xOffset: nil, yScale: nil, yOffset: number) -> UDim2),
	fromOffset: (x: number?, y: number?) -> UDim2,
	fromScale: (x: number?, y: number?) -> UDim2,
}
declare UniqueId: {
	new: () -> UniqueId,
	fromString: (value: string) -> UniqueId,
	null: UniqueId,
}
declare Vector2: {
	new: (x: number?, y: number?) -> Vector2,
	one: Vector2,
	xAxis: Vector2,
	yAxis: Vector2,
	zero: Vector2,
}
declare Vector2int16: { new: (x: number?, y: number?) -> Vector2int16 }
declare Vector3: {
	new: (x: number?, y: number?, z: number?) -> Vector3,
	fromAxis: (axis: EnumItem) -> Vector3,
	fromNormalId: (normal: EnumItem) -> Vector3,
	one: Vector3,
	xAxis: Vector3,
	yAxis: Vector3,
	zAxis: Vector3,
	zero: Vector3,
}
declare Vector3int16: { new: (x: number?, y: number?, z: number?) -> Vector3int16 }

-- Lune reflection database records. Values with runtime-dependent datatypes remain
-- unknown until narrowed by the caller; the surrounding records are fully typed.

export type DatabaseScriptability = "None" | "Custom" | "Read" | "ReadWrite" | "Write"
export type DatabasePropertyTag = "Deprecated" | "Hidden" | "NotBrowsable" | "NotReplicated" | "NotScriptable" | "ReadOnly" | "WriteOnly"
export type DatabaseClassTag = "Deprecated" | "NotBrowsable" | "NotCreatable" | "NotReplicated" | "PlayerReplicated" | "Service" | "Settings" | "UserSettings"
export type DatabaseProperty = {
	Datatype: string,
	Name: string,
	Scriptability: DatabaseScriptability,
	Tags: { DatabasePropertyTag },
}
export type DatabaseClass = {
	DefaultProperties: { [string]: unknown },
	Name: string,
	Properties: { [string]: DatabaseProperty },
	Superclass: string?,
	Tags: { DatabaseClassTag },
}
export type DatabaseEnum = {
	Items: { [string]: number },
	Name: string,
}
export type Database = {
	FindClass: (self: Database, name: string) -> DatabaseClass?,
	FindEnum: (self: Database, name: string) -> DatabaseEnum?,
	GetClass: (self: Database, name: string) -> DatabaseClass?,
	GetClassNames: (self: Database) -> { string },
	GetEnum: (self: Database, name: string) -> DatabaseEnum?,
	GetEnumNames: (self: Database) -> { string },
	Version: string,
}

declare function deserializeModel(contents: string): { Instance }
declare function deserializePlace(contents: string): DataModel
declare function getAuthCookie(raw: boolean?): string?
declare function getReflectionDatabase(): Database
declare function implementMethod<A..., R...>(className: string, methodName: string, callback: (Instance, A...) -> R...): ()
declare function implementProperty<T>(className: string, propertyName: string, getter: (Instance) -> T, setter: ((Instance, T) -> ())?): ()
declare function registerClass(className: string, superClassName: string?): ()
declare function registerService(serviceName: string): ()
declare function serializeModel(instances: { Instance }, xml: boolean?): string
declare function serializePlace(dataModel: DataModel, xml: boolean?): string
declare function studioApplicationPath(): string
declare function studioBuiltinPluginPath(): string
declare function studioContentPath(): string
declare function studioPluginPath(): string

-- Other Lune-specific globals.

type ScriptReference = {
	Name: string,
	Parent: any,
	GetFullName: (self: any) -> string,
	[string]: any,
}

declare script: ScriptReference

declare project: ScriptReference

declare executable: string?

declare function warn<T...>(...: T...)

declare task: {
	cancel: (thread: thread) -> (),
	defer: <A..., R...>(threadOrFunction: thread | (A...) -> R..., A...) -> thread,
	delay: <A..., R...>(duration: number?, threadOrFunction: thread | (A...) -> R..., A...) -> thread,
	spawn: <A..., R...>(threadOrFunction: thread | (A...) -> R..., A...) -> thread,
	wait: (duration: number?) -> number,
}
