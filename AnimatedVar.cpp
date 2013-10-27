#include "AnimatedVar.h"
#include "UIAnimationHelper.h"
#include "Common.h"
#include "atlbase.h"
#include <vector>
#include <algorithm>

namespace tjm {
namespace animation {

class AnimationManager
{
public:
	AnimationManager(AnimationClient* pInvalid);
	~AnimationManager();

	void AddClient(AnimationClient* pInvalid);
	void RemoveClient(AnimationClient* pInvalid);
	int ClientCount() { return m_pInvalid.size(); }
	void OnChange();

	void Update();
	void Kick();

	// WAM Accessors
	IUIAnimationManager* get_IUIAnimationManager() { return m_pAnimationManager; }
	IUIAnimationTimer* get_IUIAnimationTimer() { return m_pAnimationTimer; }
	IUIAnimationTransitionLibrary* get_IUIAnimationTransitionLibrary() { return m_pTransitionLibrary; }

private:
	std::vector<AnimationClient*> m_pInvalid;

	IUIAnimationManagerEventHandler* m_pHandler;
	IUIAnimationManager *m_pAnimationManager;
    IUIAnimationTimer *m_pAnimationTimer;
    IUIAnimationTransitionLibrary *m_pTransitionLibrary;
};

struct StoryBoardInternal
{
	StoryBoardInternal();
	~StoryBoardInternal();
	IUIAnimationStoryboard *m_pStoryboard;
	bool m_dirty;
	int m_count;
};

AnimationManager* g_manager;
StoryBoardInternal* g_curStoryBoard;
int g_instantChange;

void Initialize(AnimationClient* pClient)
{
	if(!g_manager) {
		g_manager = new AnimationManager(pClient);
	} else {
		g_manager->AddClient(pClient);
	}
}

void Uninitialize(AnimationClient* pClient)
{
	g_manager->RemoveClient(pClient);

	if(!g_manager->ClientCount())
	{
		delete g_manager;
		g_manager = nullptr;
	}
}

void Update()
{
	g_manager->Update();
}

void Kick()
{
	g_manager->Kick();
}

class CManagerEventHandler :
    public CUIAnimationManagerEventHandlerBase<CManagerEventHandler>
{
public:

    static HRESULT
    CreateInstance
    (
        AnimationManager *pManager,
        IUIAnimationManagerEventHandler **ppManagerEventHandler
    ) throw()
    {
        CManagerEventHandler *pManagerEventHandler;
        HRESULT hr = CUIAnimationCallbackBase::CreateInstance(
            ppManagerEventHandler,
            &pManagerEventHandler
            );
        if (SUCCEEDED(hr))
        {
            pManagerEventHandler->m_pManager = pManager;
        }
        
        return hr;
    }

    // IUIAnimationManagerEventHandler

    IFACEMETHODIMP
    OnManagerStatusChanged
    (
        UI_ANIMATION_MANAGER_STATUS newStatus,
        UI_ANIMATION_MANAGER_STATUS
    )
    {
        HRESULT hr = S_OK;

        if (newStatus == UI_ANIMATION_MANAGER_BUSY)
        {
            m_pManager->OnChange();
        }

        return hr;
    }

protected:

    CManagerEventHandler()
      : m_pManager(NULL)
    {
    }

    AnimationManager *m_pManager;
};

AnimationManager::AnimationManager(AnimationClient* pInvalid)
{
	AddClient(pInvalid);

	// Create Animation Manager
    CORt(CoCreateInstance(CLSID_UIAnimationManager, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_pAnimationManager)));
    // Create Animation Timer
    CORt(CoCreateInstance(CLSID_UIAnimationTimer, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_pAnimationTimer)));
    CORt(CoCreateInstance(CLSID_UIAnimationTransitionLibrary, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_pTransitionLibrary)));

	CORt(CManagerEventHandler::CreateInstance(this, &m_pHandler));
	CORt(m_pAnimationManager->SetManagerEventHandler(m_pHandler));
}

void AnimationManager::AddClient(AnimationClient* pInvalid)
{
	m_pInvalid.push_back(pInvalid);
}

void AnimationManager::RemoveClient(AnimationClient* pInvalid)
{
	m_pInvalid.erase(std::remove(std::begin(m_pInvalid), std::end(m_pInvalid), pInvalid), std::end(m_pInvalid));
}

void AnimationManager::OnChange() 
{
	for(auto& i : m_pInvalid) 
	{
		if(i)
			i->OnChange();
	}
}

AnimationManager::~AnimationManager()
{
	SafeRelease(&m_pHandler);
    SafeRelease(&m_pAnimationManager);
    SafeRelease(&m_pAnimationTimer);
    SafeRelease(&m_pTransitionLibrary);
}

void AnimationManager::Update()
{
    // Update the animation manager with the current time
    UI_ANIMATION_SECONDS secondsNow;
    CORt(m_pAnimationTimer->GetTime(&secondsNow));
    CORt(m_pAnimationManager->Update(secondsNow));
}

void AnimationManager::Kick()
{
    UI_ANIMATION_MANAGER_STATUS status;
    CORt(m_pAnimationManager->GetStatus(&status));
    if (status == UI_ANIMATION_MANAGER_BUSY)
    {
		OnChange();
    }

}

StoryBoardInternal::~StoryBoardInternal()
{
	// Schedule
	if(m_pStoryboard && m_dirty)
	{
		UI_ANIMATION_SECONDS secondsNow;
		CORt(g_manager->get_IUIAnimationTimer()->GetTime(&secondsNow));
		CORt(m_pStoryboard->Schedule(secondsNow));
	}

	SafeRelease(&m_pStoryboard);
}

StoryBoardInternal::StoryBoardInternal() : 
m_pStoryboard(nullptr), 
m_dirty(false),
m_count(1)
{
}

StoryBoard::StoryBoard() :
m_keyframeAnd(UI_ANIMATION_KEYFRAME_STORYBOARD_START),
m_keyframeThen(UI_ANIMATION_KEYFRAME_STORYBOARD_START),
m_thenOffset(0.0),
m_andOffset(0.0)
{
	if(g_curStoryBoard)
	{
		g_curStoryBoard->m_count++;
	} 
	else
	{
		g_curStoryBoard = new StoryBoardInternal();
	}
}

StoryBoard::~StoryBoard()
{
	if(--g_curStoryBoard->m_count == 0)
	{
		delete g_curStoryBoard;
		g_curStoryBoard = nullptr;
	}
}

StoryBoard::StoryBoard(const StoryBoard& other)
{
	m_keyframeAnd = other.m_keyframeAnd;
	m_keyframeThen = other.m_keyframeThen;
	
	++g_curStoryBoard->m_count;

	m_thenOffset = other.m_thenOffset;
	m_andOffset = other.m_andOffset;
}

StoryBoard& StoryBoard::operator=(const StoryBoard& other)
{
	if(&other != this) {
		m_keyframeAnd = other.m_keyframeAnd;
		m_keyframeThen = other.m_keyframeThen;
	
		++g_curStoryBoard->m_count;

		m_thenOffset = other.m_thenOffset;
		m_andOffset = other.m_andOffset;
	}

	return *this;
}

IUIAnimationStoryboard* StoryBoard::GetStoryboard()
{
	if(!g_curStoryBoard->m_pStoryboard)
		CORt(g_manager->get_IUIAnimationManager()->CreateStoryboard(&g_curStoryBoard->m_pStoryboard));

	return g_curStoryBoard->m_pStoryboard;
}

StoryBoard& StoryBoard::And(AnimatedVar& var, double newValue)
{
	DOUBLE oldValue;
	var.get_IUIAnimationVariable()->GetFinalValue(&oldValue);
	newValue = var.bound(newValue);
	if(oldValue != newValue)
	{
		// Schedule this variable to animate with the current start time
		IUIAnimationTransition *pTransition;
		DOUBLE duration = var.GetDuration();
		
		if(var.GetDuration() && !g_instantChange)
		{
			CORt(g_manager->get_IUIAnimationTransitionLibrary()->CreateAccelerateDecelerateTransition(
				duration,
				newValue,
				var.GetAccelerationRatio(),
				var.GetDecelerationRatio(),
				&pTransition
				));
		}
		else
		{
			CORt(g_manager->get_IUIAnimationTransitionLibrary()->CreateInstantaneousTransition(newValue, &pTransition));
		}

		var.UpdateCachedVal(newValue);

		CORt(GetStoryboard()->AddTransitionAtKeyframe(var.get_IUIAnimationVariable(), pTransition, m_keyframeAnd));

		// Update "then" keyframe
		if(m_andOffset + duration != m_thenOffset)
		{
			CORt(GetStoryboard()->AddKeyframeAfterTransition(pTransition, &m_keyframeThen));
			m_thenOffset = m_andOffset + duration;
		}

		g_curStoryBoard->m_dirty = true;
	}
	return *this;
}

StoryBoard& StoryBoard::Then(AnimatedVar& var, double newValue)
{
	DOUBLE oldValue;
	var.get_IUIAnimationVariable()->GetFinalValue(&oldValue);
	newValue = var.bound(newValue);
	if(oldValue != newValue)
	{
		// Schedule this variable to animate with the current start time
		IUIAnimationTransition *pTransition;
		DOUBLE duration = var.GetDuration();
		
		if(var.GetDuration() && !g_instantChange)
		{
			CORt(g_manager->get_IUIAnimationTransitionLibrary()->CreateAccelerateDecelerateTransition(
					duration,
					newValue,
					var.GetAccelerationRatio(),
					var.GetDecelerationRatio(),
					&pTransition
					));
		}
		else
		{
			CORt(g_manager->get_IUIAnimationTransitionLibrary()->CreateInstantaneousTransition(newValue, &pTransition));
		}

		var.UpdateCachedVal(newValue);

		CORt(GetStoryboard()->AddTransitionAtKeyframe(var.get_IUIAnimationVariable(), pTransition, m_keyframeThen));

		// Update "and" keyframe
		m_keyframeAnd = m_keyframeThen;
		m_andOffset = m_thenOffset;

		// Update "then" keyframe
		if(duration > 0.0)
		{
			CORt(GetStoryboard()->AddKeyframeAfterTransition(pTransition, &m_keyframeThen));
			m_thenOffset = m_thenOffset + duration;
		}

		g_curStoryBoard->m_dirty = true;
	}
	return *this;
}


Timer::Timer()
{
	CORt(g_manager->get_IUIAnimationManager()->CreateAnimationVariable(0.0, &m_pVariable));
}

Timer::~Timer()
{
	SafeRelease(&m_pVariable);
}

double Timer::TimeLeft() const
{
	DOUBLE value;
	m_pVariable->GetValue(&value);
	return value;
}

double Timer::PercentLeft() const
{
	return TimeLeft() / m_duration;
}

bool Timer::Zero() const
{
	return TimeLeft() == 0;
}

void Timer::Reset(double seconds)
{
	m_duration = seconds;
	DOUBLE currentVal;
	m_pVariable->GetValue(&currentVal);

	if(currentVal == seconds)
		return;

	// Instant transition to 'seconds'
	CComPtr<IUIAnimationTransition> instant;
	CORt(g_manager->get_IUIAnimationTransitionLibrary()->CreateInstantaneousTransition(seconds, &instant));

	// Linear transition to zero
	CComPtr<IUIAnimationTransition> linear;
	if(seconds != 0.0)
	{
		CORt(g_manager->get_IUIAnimationTransitionLibrary()->CreateLinearTransition(seconds, 0.0, &linear));
	}

	// Build up storyboard
	CComPtr<IUIAnimationStoryboard> sb;
	CORt(g_manager->get_IUIAnimationManager()->CreateStoryboard(&sb));

	sb->AddTransition(m_pVariable, instant);
	if(linear)
		sb->AddTransition(m_pVariable, linear);

	UI_ANIMATION_SECONDS secondsNow;
	CORt(g_manager->get_IUIAnimationTimer()->GetTime(&secondsNow));
	CORt(sb->Schedule(secondsNow));

	g_manager->Update();
}

AnimatedVar::AnimatedVar(double initialValue, UI_ANIMATION_SECONDS duration, DOUBLE accelerationRatio, DOUBLE decelerationRatio) :
m_duration(duration),
m_accelerationRatio(accelerationRatio),
m_decelerationRatio(decelerationRatio),
m_setInstant(true),
m_cachedVal(initialValue),
m_haveMin(false),
m_haveMax(false)
{
	CORt(g_manager->get_IUIAnimationManager()->CreateAnimationVariable(initialValue, &m_pVariable));
}

AnimatedVar::~AnimatedVar()
{
	SafeRelease(&m_pVariable);
}

void AnimatedVar::SetMin(double* newMin)
{
	if(newMin)
	{
		m_min = *newMin;
		m_haveMin = true;
	}
	else
	{
		m_haveMin = false;
	}
}

void AnimatedVar::SetMax(double* newMax)
{
	if(newMax)
	{
		m_max = *newMax;
		m_haveMax = true;
	}
	else
	{
		m_haveMax = false;
	}
}

double AnimatedVar::bound(double val) const
{
	if(m_haveMin && val < m_min)
		return m_min;
	if(m_haveMax && val > m_max)
		return m_max;

	return val;
}

StoryBoard AnimatedVar::SetValue(double newVal)
{
	StoryBoard storyBoard;
	storyBoard.And(*this, newVal);
	return storyBoard;
}

void AnimatedVar::UpdateCachedVal(double newVal)
{
	m_cachedVal = newVal;
	m_setInstant = (GetDuration() == 0.0 || g_instantChange);
}

double AnimatedVar::operator=(double newValue)
{
	SetValue(newValue);
	return newValue;
}

// Getting the value
double AnimatedVar::GetValue() const
{ 
	DOUBLE value;

	if(m_setInstant)
		value = bound(m_cachedVal);
	else
		CORt(m_pVariable->GetValue(&value));

	return value;
}

// Getting the value
double AnimatedVar::GetFinalValue() const
{ 
	return bound(m_cachedVal);
}

AnimatedVar::operator double() const
{
	return GetValue();
}

InstantChange::InstantChange(AnimatedVar& anim, bool occur) : 
m_anim(anim), 
m_old(anim.GetDuration()), 
m_occur(occur)
{ 
	if(m_occur) 
		m_anim.SetDuration(0.0); 
}

InstantChange::~InstantChange() 
{ 
	if(m_occur) 
		m_anim.SetDuration(m_old); 
}

AllInstant::AllInstant(bool occur) :
m_occur(occur)
{
	if(occur)
	{
		++g_instantChange;
	}
}

AllInstant::~AllInstant()
{
	if(m_occur)
	{
		--g_instantChange;
	}
}

} // end namesapce animation
} // end namespace tjm