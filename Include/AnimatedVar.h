#ifndef ANIMATEDVAR_H
#define ANIMATEDVAR_H

#ifdef ANIMATEDVAR_EXPORTS
#define ANIMATEDVAR_API __declspec(dllexport)
#else
#define ANIMATEDVAR_API __declspec(dllimport)
#endif

#include <UIAnimation.h>

namespace tjm { 
namespace animation {

// Callback interface to be notified of changes. You might want 
// to respond to this by invalidating your window, for example.
class ANIMATEDVAR_API AnimationClient
{
public:
	virtual void OnChange() = 0;
};

ANIMATEDVAR_API void Initialize(AnimationClient* pClient);
ANIMATEDVAR_API void Uninitialize(AnimationClient* pClient);
ANIMATEDVAR_API void Update();
ANIMATEDVAR_API void Kick();

class AnimationLibrary
{
public:
	AnimationLibrary(AnimationClient* pClient) : m_pClient(pClient)
	{ Initialize(m_pClient); }
	~AnimationLibrary()
	{ Uninitialize(m_pClient); }

	void Kick() { tjm::animation::Kick(); }
	void Update() { tjm::animation::Update(); }

private:
	AnimationClient* m_pClient;
};
// AnimatedVar is a double whose value changes to an assigned value over 
// a duration, using the specified acceleration and deceleration ratios.
//
// Use it whenever you want a double value animates, such as the x and y 
// location of a UI element. Use it exactly like you would a double.
//
// In practice, if you're animating two related values (such as the x and
// y value of a UI element) you'll need to use a StoryBoard.
class StoryBoard;
class ANIMATEDVAR_API AnimatedVar
{
public:
	AnimatedVar(double initialValue = 0.0, UI_ANIMATION_SECONDS duration = 0.5, 
		DOUBLE accelerationRatio = 0.5, DOUBLE decelerationRatio = 0.5);
	~AnimatedVar();

	// Bounds
	void SetMin(double* newMin);
	void SetMax(double* newMax);

	// Setting the value
	StoryBoard SetValue(double newValue);
	double operator=(double newValue);

	// Getting the value
	double GetFinalValue() const;
	double GetValue() const;
	operator double() const;

	// Animation Variables
	UI_ANIMATION_SECONDS GetDuration() const { return m_duration; }
	void SetDuration(UI_ANIMATION_SECONDS dur) { m_duration = dur; }
	DOUBLE GetAccelerationRatio() const { return m_accelerationRatio; }
	DOUBLE GetDecelerationRatio() const { return m_decelerationRatio; }

	// WAM Accessors
	IUIAnimationVariable* get_IUIAnimationVariable() { return m_pVariable; }

private:
	double bound(double val) const;
	friend StoryBoard;

	void UpdateCachedVal(DOUBLE newVal);
	bool m_setInstant;
	double m_cachedVal;
	double m_min;
	bool m_haveMin;
	double m_max;
	bool m_haveMax;

	UI_ANIMATION_SECONDS m_duration;
	DOUBLE m_accelerationRatio;
	DOUBLE m_decelerationRatio;

	IUIAnimationVariable * m_pVariable;
};

// InstantChange is a helper class for working with AnimatedVars.
// Frequently, you'll find yourself in situations where you want
// some AnimatedVar value change to happen instantly. You need to
// change the AnimatedVar's duration to 0.0, do the assign, then
// change it back. This comes up enough that the InstantChange 
// helper class was created to do exactly that. An example:
//
//     void ModifyFoo(double newValue, bool happenRightAway)
//     {
//         InstantChange(m_foo, happenRightAway);
//         m_foo = newValue;
//     }
//
// If 'occur' is false, InstantChange does nothing. If occur is 
// true, the old value of duration is remembered, the duration is
// set to 0.0, and in the destructor the old duration is restored.
class ANIMATEDVAR_API InstantChange
{
public:
	InstantChange(AnimatedVar& anim, bool occur);
	~InstantChange();

private:
	bool m_occur;
	AnimatedVar& m_anim;
	UI_ANIMATION_SECONDS m_old;
};

// Similar to InstantChange, but affects all animations everywhere
class ANIMATEDVAR_API AllInstant
{
public:
	AllInstant(bool occur);
	~AllInstant();

private:
	bool m_occur;
};

// A StoryBoard is the object used to chain together variable changes.
// Let's say you have two AnimatedVars m_x and m_y both set to 0, and do this:
//
//     m_x = 100.0;
//     m_y = 100.0;
//
// You want m_x and m_y to change in lockstep from 0 to 100.0 (or, in the UI,
// move in exactly a 45 degree angle from 0,0 to 100,100). But, sadly, that's not
// what happens. The two animations are started independantly, and it actually
// takes time to setup the animation for m_x, so m_y will be a few ms behind.
// This could show up as a few pixels on a few frames.
//
// Storyboards are the solution. Storyboards batch up animations, and start then
// all at once. So you really want to do this:
//
//     Storyboard b;
//     b.And(m_x, 100.0);
//     b.And(m_y, 100.0);
//
// The destructor of Storyboard triggers the animation to start, and things look
// as you'd expect. 
//
// You can also schedule animations to happen in sequence:
//
//     b.And(m_x, 100.0);
//     b.Then(m_y, 100.0);
//
// StoryBoard's And and Then functions actually return a reference to itself, to
// allow more natural syntax:
//
//     b.And(m_x, 100.0).Then(m_y, 100.0);
//
// Or usually,
// 
//     b.And(m_a, 100.0)
//      .And(m_b, 100.0)
//      .And(m_c, 100.0)
//        .Then(m_d, 100.0)
//        .And(m_e, 100.0)
//          .Then(m_a, 100.0);
//
// StoryBoard keeps two internal time positions: the "current" position, and the
// "then" position. When you "And" a change, it is scheduled at the current position,
// and updates the then position to point to the moment when it occurs.
//
// One more cool thing: StoryBoards are actually copyable and assignable, and the
// underlying animation is shared between them - but each instance of StoryBoard keeps
// it's own unique And and Then position pointers. Without this feature, something like
// this would be impossible:
//
//     Storyboard a;
//     a.And(m_a, 100.0);
//     Storyboard b(a); // same underlying animation
//     a.Then(m_b, 100.0).Then(m_d, 100.0); // m_b duration = 10 seconds
//     b.Then(m_c, 100.0).Then(m_e, 100.0); // m_c duration = 5 seconds;
// 
// That'll produce something like this:
// t ------------------------------->
// ====m_a====
//            ====m_b====
//                       ====m_d====
//            ==m_c==
//                   ====m_e====
//
// Really complex things are possible with StoryBoards. Have fun.
struct StoryBoardInternal;
class ANIMATEDVAR_API StoryBoard
{
public:
	StoryBoard();

	~StoryBoard();
	StoryBoard(const StoryBoard& other);
	StoryBoard& operator=(const StoryBoard& other);

	StoryBoard& And(AnimatedVar& v, double newValue);
	StoryBoard& Then(AnimatedVar& v, double newValue);

private:
	IUIAnimationStoryboard* GetStoryboard();

	UI_ANIMATION_KEYFRAME m_keyframeAnd;
	UI_ANIMATION_KEYFRAME m_keyframeThen;
	double m_thenOffset;
	double m_andOffset;
};

// Timer is a stop watch. You call Reset with a duration, and
// it immediately begins counting down to zero. You can query
// the time left (in seconds, or as a percent).
//
class ANIMATEDVAR_API Timer
{
public:
	explicit Timer();
	~Timer();

	bool Zero() const;
	double TimeLeft() const;
	double PercentLeft() const;

	void Reset(double seconds);
private:
	double m_duration;
	Timer(const Timer&); // = delete

	IUIAnimationVariable* m_pVariable;
};

} // end namespace animation
} // end namespace tjm
#endif
