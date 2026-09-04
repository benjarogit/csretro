//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdarg.h>
#include <stdio.h>

#include <vgui/ISurfaceNext.h>
#include <vgui/ISchemeNext.h>
#include <KeyValues.h>

#include <vgui_controls/Image.h>
#include <vgui_controls/CheckButton.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui2;

void CheckImage::Paint()
{
	// CS Retro: draw the checkbox as primitives. The old Marlett glyphs are too
	// blocky on the FreeType-backed menu surface and do not match the smoother
	// CS:S-style dialog chrome.
	const int boxX = 2;
	const int boxY = 2;
	const int box = 10;

	if (_CheckButton->IsEnabled() && _CheckButton->IsCheckButtonCheckable() )
	{
		DrawSetColor(_bgColor);
	}
	else
	{
		DrawSetColor(_CheckButton->GetDisabledBgColor());
	}
	DrawFilledRect(boxX + 1, boxY + 1, boxX + box, boxY + box);

	DrawSetColor(_borderColor1);
	DrawOutlinedRect(boxX, boxY, boxX + box + 1, boxY + box + 1);
	DrawSetColor(_borderColor2);
	DrawLine(boxX + 1, boxY + box + 1, boxX + box + 1, boxY + box + 1);
	DrawLine(boxX + box + 1, boxY + 1, boxX + box + 1, boxY + box + 1);

	if (_CheckButton->IsSelected())
	{
		Color check = _checkColor;
		if ( !_CheckButton->IsEnabled() )
		{
			check = _CheckButton->GetDisabledFgColor();
		}

		DrawSetColor(check);
		DrawLine(boxX + 2, boxY + 5, boxX + 4, boxY + 7);
		DrawLine(boxX + 3, boxY + 5, boxX + 5, boxY + 7);
		DrawLine(boxX + 4, boxY + 7, boxX + 8, boxY + 2);
		DrawLine(boxX + 5, boxY + 7, boxX + 9, boxY + 2);
	}
}

DECLARE_BUILD_FACTORY_DEFAULT_TEXT( CheckButton, CheckButton );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CheckButton::CheckButton(Panel *parent, const char *panelName, const char *text) : ToggleButton(parent, panelName, text)
{
 	SetContentAlignment(a_west);
	SetButtonActivationType(ACTIVATE_ONPRESSED);
	m_bCheckButtonCheckable = true;
	m_bStaySelectedOnClick = true;

	// create the image
	_checkBoxImage = new CheckImage(this);

	SetTextImageIndex(1);
	SetImageAtIndex(0, _checkBoxImage, CHECK_INSET);

	_selectedFgColor = Color( 196, 181, 80, 255 );
	_disabledFgColor = Color(130, 130, 130, 255);
	_disabledBgColor = Color(62, 70, 55, 255);
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CheckButton::~CheckButton()
{
	delete _checkBoxImage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CheckButton::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetDefaultColor( GetSchemeColor("CheckButton.TextColor", pScheme), GetBgColor() );

	auto bwSelectedFgColor = GetSchemeColor("BrightControlText", GetSchemeColor("ControlText", pScheme), pScheme);
	_selectedFgColor = GetSchemeColor("CheckButton.SelectedTextColor", bwSelectedFgColor, pScheme);

	_checkBoxImage->_bgColor = GetSchemeColor("CheckButton.BgColor", GetSchemeColor("CheckBgColor", Color(62, 70, 55, 255), pScheme), pScheme);
	_checkBoxImage->_borderColor1 = GetSchemeColor("CheckButton.Border1", GetSchemeColor("CheckButtonBorder1", Color(20, 20, 20, 0), pScheme), pScheme);
	_checkBoxImage->_borderColor2 = GetSchemeColor("CheckButton.Border2", GetSchemeColor("CheckButtonBorder2", Color(90, 90, 90, 0), pScheme), pScheme);
	_checkBoxImage->_checkColor = GetSchemeColor("CheckButton.Check", GetSchemeColor("CheckButtonCheck", Color(20, 20, 20, 0), pScheme), pScheme);
	
	_disabledFgColor = GetSchemeColor("CheckButton.DisabledFgColor", _defaultFgColor, pScheme);
	_disabledBgColor = GetSchemeColor("CheckButton.DisabledBgColor", _defaultBgColor, pScheme);

	Color bgArmedColor = GetSchemeColor( "CheckButton.ArmedBgColor", _defaultBgColor, pScheme); 
	SetArmedColor( GetFgColor(), bgArmedColor );

	Color bgDepressedColor = GetSchemeColor( "CheckButton.DepressedBgColor", _defaultBgColor, pScheme); 
	SetDepressedColor( GetFgColor(), bgDepressedColor );

	_highlightFgColor = GetSchemeColor( "CheckButton.HighlightFgColor", bwSelectedFgColor, pScheme); 

	SetContentAlignment(Label::a_west);

	_checkBoxImage->SetFont( pScheme->GetFont("Marlett", IsProportional()) );
	_checkBoxImage->ResizeImageToContent();
	SetImageAtIndex(0, _checkBoxImage, CHECK_INSET);

	// don't draw a background
	SetPaintBackgroundEnabled(false);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
IBorder *CheckButton::GetBorder(bool depressed, bool armed, bool selected, bool keyfocus)
{
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Check the button
//-----------------------------------------------------------------------------
void CheckButton::SetSelected(bool state)
{
	if (m_bCheckButtonCheckable)
	{
		// send a message saying we've been checked
		KeyValues *msg = new KeyValues("CheckButtonChecked", "state", (int)state);
		PostActionSignal(msg);
		
		BaseClass::SetSelected(state);
	}
}

void CheckButton::SilentSetSelected(bool state)
{
	if (m_bCheckButtonCheckable)
	{
		BaseClass::SetSelected(state);
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets whether or not the state of the check can be changed
//-----------------------------------------------------------------------------
void CheckButton::SetCheckButtonCheckable(bool state)
{
	m_bCheckButtonCheckable = state;
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Gets a different foreground text color if we are selected
//-----------------------------------------------------------------------------
#ifdef _X360
Color CheckButton::GetButtonFgColor()
{
	if (HasFocus())
	{
		return _selectedFgColor;
	}

	return BaseClass::GetButtonFgColor();
}
#else
Color CheckButton::GetButtonFgColor()
{
	if ( IsArmed() )
	{
		return _highlightFgColor;
	}

	if (IsSelected())
	{
		return _selectedFgColor;
	}

	return BaseClass::GetButtonFgColor();
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CheckButton::OnCheckButtonChecked(Panel *panel)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CheckButton::SetHighlightColor(Color fgColor)
{
	if ( _highlightFgColor != fgColor )
	{
		_highlightFgColor = fgColor;

		InvalidateLayout(false);
	}
}
