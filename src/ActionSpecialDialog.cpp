
/*******************************************************************
 * SLADE - It's a Doom Editor
 * Copyright (C) 2008-2014 Simon Judd
 *
 * Email:       sirjuddington@gmail.com
 * Web:         http://slade.mancubus.net
 * Filename:    ActionSpecialDialog.cpp
 * Description: A dialog that allows selection of an action special
 *              (and other related classes)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *******************************************************************/


/*******************************************************************
 * INCLUDES
 *******************************************************************/
#include "Main.h"
#include "WxStuff.h"
#include "ActionSpecialDialog.h"
#include "GameConfiguration.h"
#include "GenLineSpecialPanel.h"
#include "MapEditorWindow.h"
#include "NumberTextCtrl.h"
#include "STabCtrl.h"
#include <wx/gbsizer.h>
#include <wx/window.h>
#undef min
#undef max
#include <wx/valnum.h>


/*******************************************************************
 * ACTIONSPECIALTREEVIEW CLASS FUNCTIONS
 *******************************************************************/

/* ActionSpecialTreeView::ActionSpecialTreeView
 * ActionSpecialTreeView class constructor
 *******************************************************************/
ActionSpecialTreeView::ActionSpecialTreeView(wxWindow* parent) : wxDataViewTreeCtrl(parent, -1)
{
	parent_dialog = NULL;

	// Create root item
	root = wxDataViewItem(0);

	// Add 'None'
	item_none = AppendItem(root, "0: None");

	// Computing the minimum width of the tree is slightly complicated, since
	// wx doesn't expose it to us directly
	wxClientDC dc(this);
	dc.SetFont(GetFont());
	wxSize textsize;

	// Populate tree
	vector<as_t> specials = theGameConfiguration->allActionSpecials();
	std::sort(specials.begin(), specials.end());
	for (unsigned a = 0; a < specials.size(); a++)
	{
		string label = S_FMT("%d: %s", specials[a].number, specials[a].special->getName());
		AppendItem(getGroup(specials[a].special->getGroup()), label);
		textsize.IncTo(dc.GetTextExtent(label));
	}
	Expand(root);

	// Bind events
	Bind(wxEVT_DATAVIEW_ITEM_START_EDITING, &ActionSpecialTreeView::onItemEdit, this);
	Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &ActionSpecialTreeView::onItemActivated, this);

	// 64 is an arbitrary fudge factor -- should be at least the width of a
	// scrollbar plus the expand icons plus any extra padding
	int min_width = textsize.GetWidth() + GetIndent() + 64;
	SetMinSize(wxSize(min_width, 200));
}

/* ActionSpecialTreeView::~ActionSpecialTreeView
 * ActionSpecialTreeView class destructor
 *******************************************************************/
ActionSpecialTreeView::~ActionSpecialTreeView()
{
}

/* ActionSpecialTreeView::specialNumber
 * Returns the action special value for [item]
 *******************************************************************/
int ActionSpecialTreeView::specialNumber(wxDataViewItem item)
{
	string num = GetItemText(item).BeforeFirst(':');
	long s;
	if (!num.ToLong(&s))
		s = -1;

	return s;
}

/* ActionSpecialTreeView::showSpecial
 * Finds the item for [special], selects it and ensures it is shown
 *******************************************************************/
void ActionSpecialTreeView::showSpecial(int special, bool focus)
{
	if (special == 0)
	{
		EnsureVisible(item_none);
		Select(item_none);
		if (focus)
			SetFocus();
		return;
	}

	// Go through item groups
	for (unsigned a = 0; a < groups.size(); a++)
	{
		// Go through group items
		for (int b = 0; b < GetChildCount(groups[a].item); b++)
		{
			wxDataViewItem item = GetNthChild(groups[a].item, b);

			// Select+show if match
			if (specialNumber(item) == special)
			{
				EnsureVisible(item);
				Select(item);
				if (focus)
					SetFocus();
				return;
			}
		}
	}
}

/* ActionSpecialTreeView::selectedSpecial
 * Returns the currently selected action special value
 *******************************************************************/
int ActionSpecialTreeView::selectedSpecial()
{
	wxDataViewItem item = GetSelection();
	if (item.IsOk())
		return specialNumber(item);
	else
		return -1;
}

/* ActionSpecialTreeView::getGroup
 * Returns the parent wxDataViewItem representing action special
 * group [group]
 *******************************************************************/
wxDataViewItem ActionSpecialTreeView::getGroup(string group)
{
	// Check if group was already made
	for (unsigned a = 0; a < groups.size(); a++)
	{
		if (group == groups[a].name)
			return groups[a].item;
	}

	// Split group into subgroups
	wxArrayString path = wxSplit(group, '/');

	// Create group needed
	wxDataViewItem current = root;
	string fullpath = "";
	for (unsigned p = 0; p < path.size(); p++)
	{
		if (p > 0) fullpath += "/";
		fullpath += path[p];

		bool found = false;
		for (unsigned a = 0; a < groups.size(); a++)
		{
			if (groups[a].name == fullpath)
			{
				current = groups[a].item;
				found = true;
				break;
			}
		}

		if (!found)
		{
			current = AppendContainer(current, path[p], -1, 1);
			groups.push_back(astv_group_t(current, fullpath));
		}
	}

	return current;
}


/*******************************************************************
 * ACTIONSPECIALTREEVIEW CLASS EVENTS
 *******************************************************************/

/* ActionSpecialTreeView::onItemEdit
 * Called when a tree item label is edited
 *******************************************************************/
void ActionSpecialTreeView::onItemEdit(wxDataViewEvent& e)
{
	e.Veto();
}

/* ActionSpecialTreeView::onItemActivated
 * Called when a tree item is activated
 *******************************************************************/
void ActionSpecialTreeView::onItemActivated(wxDataViewEvent& e)
{
	if (parent_dialog)
		parent_dialog->EndModal(wxID_OK);
}


/*******************************************************************
 * ARGSPANEL CLASS FUNCTIONS
 *******************************************************************/

/* ArgsControl
 * Helper class that contains controls specific to a particular
 * argument.  Usually this is a text box, but some args take one of
 * a list of choices, flags, etc.
 *******************************************************************/
class ArgsControl : public wxPanel
{
public:
	ArgsControl(wxWindow* parent) : wxPanel(parent, -1)
	{
		SetSizer(new wxBoxSizer(wxVERTICAL));
	}
	~ArgsControl() {}

	virtual long getArgValue() = 0;
	virtual void setArgValue(long val) = 0;
};

/* ArgsTextControl
 * Trivial case of an arg control: a text box that can hold a number
 * from 0 to 255.
 *******************************************************************/
class ArgsTextControl : public ArgsControl
{
protected:
	// This is the control holding the "real" value
	wxTextCtrl* text_control;

public:
	ArgsTextControl(wxWindow* parent) : ArgsControl(parent)
	{
		text_control = new wxTextCtrl(this, -1, "", wxDefaultPosition, wxSize(40, -1));
		text_control->SetValidator(wxIntegerValidator<unsigned char>());
		GetSizer()->Add(text_control, wxSizerFlags().Expand());
	}

	// Get the value of the argument from the textbox
	long getArgValue()
	{
		wxString val = text_control->GetValue();

		// Empty string means ignore it
		if (val == "")
			return -1;

		long ret;
		val.ToLong(&ret);
		return ret;
	}

	// Set the value in the textbox
	void setArgValue(long val)
	{
		if (val < 0)
			text_control->ChangeValue("");
		else
			text_control->ChangeValue(S_FMT("%ld", val));
	}
};

/* ArgsChoiceControl
 * Combo box for an argument that takes one of a set of predefined
 * values.
 *******************************************************************/
// Helper for the combo box.  wxIntegerValidator, by default, will erase the
// entire combo box if one of the labeled numbers is selected, because the
// label isn't a valid number.
template<typename T>
class ComboBoxAwareIntegerValidator : public wxIntegerValidator<T>
{
public:
	ComboBoxAwareIntegerValidator() : wxIntegerValidator<T>() {}
	ComboBoxAwareIntegerValidator(const ComboBoxAwareIntegerValidator& validator)
		: wxIntegerValidator<T>(validator) {}

	virtual wxObject* Clone() const { return new ComboBoxAwareIntegerValidator(*this); }

	virtual wxString NormalizeString(const wxString& s) const
	{
		// If there's a valid selection in the combobox, don't "normalize".
		// This is a highly inappropriate place for this check, but everything
		// else is private and non-virtual.
		wxComboBox* control = static_cast<wxComboBox*>(wxNumValidatorBase::GetTextEntry());
		if (control->GetSelection() != wxNOT_FOUND)
			return s;

		return wxIntegerValidator<T>::NormalizeString(s);
	}
};

class ArgsChoiceControl : public ArgsControl
{
private:
	wxComboBox*			choice_control;
	vector<arg_val_t>&	choices;

public:
	ArgsChoiceControl(wxWindow* parent, vector<arg_val_t>& choices)
		: ArgsControl(parent), choices(choices)
	{
		choice_control = new wxComboBox(this, -1, "", wxDefaultPosition, wxSize(100, -1));
		choice_control->SetValidator(ComboBoxAwareIntegerValidator<unsigned char>());

		for (unsigned i = 0; i < choices.size(); i++)
		{
			choice_control->Append(
				S_FMT("%d: %s", choices[i].value, choices[i].name));
		}

		GetSizer()->Add(choice_control, wxSizerFlags().Expand());
		Fit();
	}

	long getArgValue()
	{
		int selected = choice_control->GetSelection();
		if (selected == wxNOT_FOUND)
		{
			// No match.  User must have entered a value themselves
			string val = choice_control->GetValue();

			// Empty string means ignore it
			if (val == "")
				return -1;

			long ret;
			val.ToLong(&ret);
			return ret;

		}
		else
		{
			return choices[selected].value;
		}
	}

	void setArgValue(long val)
	{
		if (val < 0)
		{
			choice_control->ChangeValue("");
			return;
		}

		// Look for a name for this value
		for (unsigned i = 0; i < choices.size(); i++)
		{
			if (val == choices[i].value)
			{
				choice_control->SetSelection(i);
				return;
			}
		}
		choice_control->ChangeValue(S_FMT("%ld", val));
	}
};

/* ArgsFlagsControl
 * Set of checkboxes, for an argument that contains flags.
 *******************************************************************/
class ArgsFlagsControl : public ArgsTextControl
{
private:
	// Reference to the arg's custom_flags
	vector<arg_val_t>&	flags;
	// Parallel vector of bitmasks for the groups each flag belongs to, or 0
	// for an independent flag
	vector<int>			flag_to_bit_group;
	// Parallel vector of the checkboxes and radio buttons we create
	vector<wxControl*>	controls;

	bool isPowerOfTwo(long n) { return (n & (n - 1)) == 0; }

	/* ArgsFlagsControl::addControl
	 * Add a checkbox or radio button to the sizer, and perform some
	 * bookkeeping.
	*******************************************************************/
	void addControl(wxControl* control, int index, int group)
	{
		GetSizer()->Add(control);
		controls[index] = control;
		flag_to_bit_group[index] = group;
		control->Bind(wxEVT_CHECKBOX, &ArgsFlagsControl::onCheck, this);
		control->Bind(wxEVT_RADIOBUTTON, &ArgsFlagsControl::onCheck, this);
	}

	/* ArgsFlagsControl::onCheck
	 * Event handler called when a checkbox or radio button is toggled.
	 * Update the value in the textbox.
	*******************************************************************/
	void onCheck(wxCommandEvent& event)
	{
		// Note that this function does NOT recompute the arg value from
		// scratch!  There might be newer flags we don't know about, and
		// blindly erasing them would be rude.  Instead, only twiddle the
		// single flag corresponding to this checkbox.
		event.Skip();

		int val = getArgValue();
		if (val < 0)
			return;

		// Doesn't matter what type of pointer this is; only need it to find
		// the flag index
		wxObject* control = event.GetEventObject();
		for (unsigned i = 0; i < flags.size(); i++)
		{
			if (controls[i] == control)
			{
				// Remove the entire group
				if (flag_to_bit_group[i])
					val &= ~flag_to_bit_group[i];
				else
					val &= ~flags[i].value;

				// Then re-add if appropriate
				if (event.IsChecked())
					val |= flags[i].value;
				ArgsTextControl::setArgValue(val);
				return;
			}
		}
	}

	/* ArgsFlagsControl::onKeypress
	 * Event handler called when a key is pressed in the textbox.  Refresh
	 * all the flag states.
	*******************************************************************/
	void onKeypress(wxKeyEvent& event)
	{
		event.Skip();
		updateCheckState(getArgValue());
	}

	/* ArgsFlagsControl::updateCheckState
	 * Do the actual work of updating the checkbox states.
	*******************************************************************/
	void updateCheckState(long val)
	{
		for (unsigned i = 0; i < flags.size(); i++)
		{
			if (flag_to_bit_group[i])
			{
				bool checked = (val >= 0 && (val & flag_to_bit_group[i]) == flags[i].value);
				static_cast<wxRadioButton*>(controls[i])->SetValue(checked);
			}
			else
			{
				bool checked = (val >= 0 && (val & flags[i].value) == flags[i].value);
				static_cast<wxCheckBox*>(controls[i])->SetValue(checked);
			}
		}
	}

public:
	ArgsFlagsControl(wxWindow* parent, vector<arg_val_t>& flags)
		: ArgsTextControl(parent), flags(flags), flag_to_bit_group(flags.size(), 0), controls(flags.size(), NULL)
	{
		text_control->Bind(wxEVT_KEY_UP, &ArgsFlagsControl::onKeypress, this);

		wxControl* control;
		wxSizer* sizer = GetSizer();

		// Sometimes multiple bits are used for a set of more than two flags.
		// For example, if 3 is a flag, then it must be one of /four/ flags
		// along with values 0, 1, and 2.  In such cases, we need radio buttons
		// instead of a checkbox.
		// This is not as robust as it could be, but to my knowledge, the only
		// place this gets used is the "type" argument to ZDoom's
		// Sector_Set3DFloor, where the first two bits are an enum.
		vector<int> bit_groups;
		for (unsigned i = 0; i < flags.size(); i++)
		{
			int value = flags[i].value;
			if (isPowerOfTwo(value))
				continue;

			bool found_match = false;
			for (unsigned j = 0; j < bit_groups.size(); j++)
			{
				if (bit_groups[j] & value) {
					bit_groups[j] |= value;
					found_match = true;
					break;
				}
			}
			if (!found_match)
				bit_groups.push_back(value);
		}

		vector<bool> flag_done(flags.size(), false);
		for (unsigned i = 0; i < flags.size(); i++)
		{
			if (flag_done[i])
				continue;

			// Check if this flag is part of a group
			int group = 0;
			int check_against = flags[i].value;
			// Special case: if the value is 0, it has no bits, so assume it's
			// part of the next flag's group
			if (flags[i].value == 0 && i < flags.size() - 1)
				check_against = flags[i + 1].value;
			for (unsigned j = 0; j < bit_groups.size(); j++)
				if (bit_groups[j] & check_against) {
					group = bit_groups[j];
					break;
				}

			if (group)
			{
				addControl(
					new wxRadioButton(this, -1, S_FMT("%d: %s", flags[i].value, flags[i].name),
						wxDefaultPosition, wxDefaultSize, wxRB_GROUP),
					i, group);
				// Find all the other (later) flags that are part of this same bit group
				for (unsigned ii = i + 1; ii < flags.size(); ii++)
				{
					if (flag_done[ii])
						continue;
					if (flags[ii].value & group)
					{
						addControl(
							new wxRadioButton(this, -1, S_FMT("%d: %s", flags[ii].value, flags[ii].name)),
							ii, group);
						flag_done[ii] = true;
					}
				}
			}
			else  // not in a group
			{
				control = new wxCheckBox(this, -1, S_FMT("%d: %s", flags[i].value, flags[i].name));
				addControl(control, i, 0);
			}
		}

		Fit();
	}

	void setArgValue(long val)
	{
		ArgsTextControl::setArgValue(val);
		updateCheckState(val);
	}
};

/* ArgsSpeedControl
 * Arg control that shows a slider for selecting a flat movement speed.
 *******************************************************************/
class ArgsSpeedControl : public ArgsTextControl
{
protected:
	wxSlider* slider_control;
	wxStaticText* speed_label;

	void onSlide(wxCommandEvent& event)
	{
		syncControls(slider_control->GetValue());
	}

	void syncControls(int value)
	{
		ArgsTextControl::setArgValue(value);

		if (value < 0)
		{
			slider_control->SetValue(0);
			speed_label->SetLabel("");
		}
		else
		{
			slider_control->SetValue(value);
			speed_label->SetLabel(S_FMT(
				"%s (%.1f units per tic, %.1f units per sec)",
				arg_t::speedLabel(value), value / 8.0,
				// A tic is 28ms, slightly less than 1/35 of a second
				value / 8.0 * 1000.0 / 28.0
			));
		}
	}

public:
	ArgsSpeedControl(wxWindow* parent) : ArgsTextControl(parent)
	{
		wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);

		GetSizer()->Detach(text_control);
		row->Add(text_control, wxSizerFlags(1).Expand());
		speed_label = new wxStaticText(this, -1, "");
		row->AddSpacer(4);
		row->Add(speed_label, wxSizerFlags(4).Align(wxALIGN_CENTER_VERTICAL));
		GetSizer()->Add(row, wxSizerFlags(1).Expand());

		slider_control = new wxSlider(this, -1, 0, 0, 255);
		slider_control->SetLineSize(2);
		slider_control->SetPageSize(8);
		// These are the generalized Boom speeds
		slider_control->SetTick(8);
		slider_control->SetTick(16);
		slider_control->SetTick(32);
		slider_control->SetTick(64);
		slider_control->Bind(wxEVT_SLIDER, &ArgsSpeedControl::onSlide, this);
		GetSizer()->Add(slider_control, wxSizerFlags(1).Expand());

		// The label has its longest value at 0, which makes for an appropriate
		// minimum size
		syncControls(0);

		Fit();
	}

	// Set the value in the textbox
	void setArgValue(long val)
	{
		syncControls(val);
	}
};

/* ArgsPanel::ArgsPanel
 * ArgsPanel class constructor
 *******************************************************************/
ArgsPanel::ArgsPanel(wxWindow* parent)
: wxScrolled<wxPanel>(parent, -1, wxDefaultPosition, wxDefaultSize, wxVSCROLL)
{
	// Setup sizer
	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(sizer);

	// Add arg controls
	fg_sizer = new wxFlexGridSizer(2, 4, 4);
	fg_sizer->AddGrowableCol(1);
	sizer->Add(fg_sizer, 1, wxEXPAND|wxALL, 4);

	for (unsigned a = 0; a < 5; a++)
	{
		label_args[a] = new wxStaticText(this, -1, "");
		control_args[a] = NULL;
		label_args_desc[a] = new wxStaticText(this, -1, "", wxDefaultPosition, wxSize(100, -1));
	}

	// Set up vertical scrollbar
	SetScrollRate(0, 10);

	Bind(wxEVT_SIZE, &ArgsPanel::onSize, this);
}

/* ArgsPanel::setup
 * Sets up the arg names and descriptions from specification in [args]
 *******************************************************************/
void ArgsPanel::setup(argspec_t* args)
{
	// Reset stuff
	fg_sizer->Clear();
	for (unsigned a = 0; a < 5; a++)
	{
		if (control_args[a])
			control_args[a]->Destroy();
		control_args[a] = NULL;
		label_args[a]->SetLabelText(S_FMT("Arg %d:", a + 1));
		label_args_desc[a]->Show(false);
	}

	// Setup layout
	int row = 0;
	for (unsigned a = 0; a < 5; a++)
	{
		arg_t& arg = args->getArg(a);
		bool has_desc = false;
		
		if ((int)a < args->count) {
			has_desc = !arg.desc.IsEmpty();

			if (arg.type == ARGT_CHOICE)
				control_args[a] = new ArgsChoiceControl(this, arg.custom_values);
			else if (arg.type == ARGT_FLAGS)
				control_args[a] = new ArgsFlagsControl(this, arg.custom_flags);
			else if (arg.type == ARGT_SPEED)
				control_args[a] = new ArgsSpeedControl(this);
			else
				control_args[a] = new ArgsTextControl(this);
		}
		else {
			control_args[a] = new ArgsTextControl(this);
		}

		// Arg name
		label_args[a]->SetLabelText(S_FMT("%s:", arg.name));
		fg_sizer->Add(label_args[a], wxSizerFlags().Align(wxALIGN_TOP|wxALIGN_RIGHT).Border(wxALL, 4));

		// Arg value
		fg_sizer->Add(control_args[a], wxSizerFlags().Expand());
		
		// Arg description
		if (has_desc)
		{
			// Add an empty spacer to the first column
			fg_sizer->Add(0, 0);
			fg_sizer->Add(label_args_desc[a], wxSizerFlags().Expand());
		}
	}

	// We may have changed the minimum size of the window by adding new big
	// controls, so we need to ask the top-level parent to recompute its
	// minimum size
	wxWindow* toplevel = this;
	while (!toplevel->IsTopLevel() && toplevel->GetParent())
		toplevel = toplevel->GetParent();
	wxSizer* toplevel_sizer = toplevel->GetSizer();
	if (toplevel_sizer)
	{
		// This is more or less what SetSizerAndFit does, but without resizing
		// the window if not necessary
		toplevel->SetMinClientSize(
			toplevel_sizer->ComputeFittingClientSize(toplevel));
		wxSize toplevel_size = toplevel->GetSize();
		wxSize toplevel_best = toplevel_size;
		toplevel_best.IncTo(toplevel->GetBestSize());
		if (toplevel_best != toplevel_size)
			toplevel->SetSize(toplevel_best);
	}

	// Set the label text last, so very long labels will wrap naturally and not
	// force the window to be ridiculously wide
	Layout();
	int available_width = fg_sizer->GetColWidths()[1];
	for (int a = 0; a < args->count; a++)
	{
		arg_t& arg = args->getArg(a);

		if (!arg.desc.IsEmpty())
		{
			label_args_desc[a]->Show(true);
			label_args_desc[a]->SetLabelText(arg.desc);
			label_args_desc[a]->Wrap(available_width);
		}
	}

	FitInside();  // for wxScrolled's benefit
}

/* ArgsPanel::setValues
 * Sets the arg values
 *******************************************************************/
void ArgsPanel::setValues(int args[5])
{
	for (unsigned a = 0; a < 5; a++)
	{
		control_args[a]->setArgValue(args[a]);
	}
}

/* ArgsPanel::getArgValue
 * Returns the current value for arg [index]
 *******************************************************************/
int ArgsPanel::getArgValue(int index)
{
	// Check index
	if (index < 0 || index > 4)
		return -1;

	return control_args[index]->getArgValue();
}

/* ArgsPanel::onSize
 * Rewrap the descriptions when the panel is resized
 *******************************************************************/
void ArgsPanel::onSize(wxSizeEvent& event)
{
	event.Skip();

	Layout();
	if (fg_sizer->GetColWidths().size() > 1)
	{
		int available_width = fg_sizer->GetColWidths()[1];
		for (int a = 0; a < 5; a++)
		{
			// Wrap() puts hard newlines in the label, so we need to remove them
			wxString label = label_args_desc[a]->GetLabelText();
			label.Replace("\n", " ");
			label_args_desc[a]->SetLabelText(label);
			label_args_desc[a]->Wrap(available_width);
		}
	}
}



/*******************************************************************
 * ACTIONSPECIALPANEL CLASS FUNCTIONS
 *******************************************************************/

/* ActionSpecialPanel::ActionSpecialPanel
 * ActionSpecialPanel class constructor
 *******************************************************************/
ActionSpecialPanel::ActionSpecialPanel(wxWindow* parent, bool trigger) : wxPanel(parent, -1)
{
	panel_args = NULL;
	choice_trigger = NULL;
	show_trigger = trigger;

	// Setup layout
	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

	if (theGameConfiguration->isBoom())
	{
		// Action Special radio button
		wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);
		sizer->Add(hbox, 0, wxEXPAND|wxLEFT|wxTOP|wxRIGHT, 4);
		rb_special = new wxRadioButton(this, -1, "Action Special", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
		hbox->Add(rb_special, 0, wxEXPAND|wxRIGHT, 8);

		// Generalised Special radio button
		rb_generalised = new wxRadioButton(this, -1, "Generalised Special");
		hbox->Add(rb_generalised, 0, wxEXPAND);

		// Boom generalised line special panel
		panel_gen_specials = new GenLineSpecialPanel(this);
		panel_gen_specials->Show(false);

		// Bind events
		rb_special->Bind(wxEVT_RADIOBUTTON, &ActionSpecialPanel::onRadioButtonChanged, this);
		rb_generalised->Bind(wxEVT_RADIOBUTTON, &ActionSpecialPanel::onRadioButtonChanged, this);
	}

	// Action specials tree
	setupSpecialPanel();
	sizer->Add(panel_action_special, 1, wxEXPAND|wxALL, 4);

	SetSizerAndFit(sizer);

	// Bind events
	tree_specials->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &ActionSpecialPanel::onSpecialSelectionChanged, this);
	tree_specials->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &ActionSpecialPanel::onSpecialItemActivated, this);
}

/* ActionSpecialPanel::~ActionSpecialPanel
 * ActionSpecialPanel class destructor
 *******************************************************************/
ActionSpecialPanel::~ActionSpecialPanel()
{
}

WX_DECLARE_STRING_HASH_MAP(wxFlexGridSizer*, NamedFlexGridMap);

/* ActionSpecialPanel::setupSpecialPanel
 * Creates and sets up the action special panel
 *******************************************************************/
void ActionSpecialPanel::setupSpecialPanel()
{
	// Create panel
	panel_action_special = new wxPanel(this, -1);
	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

	// Special box
	text_special = new NumberTextCtrl(panel_action_special);
	sizer->Add(text_special, 0, wxEXPAND|wxALL, 4);
	text_special->Bind(wxEVT_TEXT, &ActionSpecialPanel::onSpecialTextChanged, this);

	// Action specials tree
	tree_specials = new ActionSpecialTreeView(panel_action_special);
	sizer->Add(tree_specials, 1, wxEXPAND|wxALL, 4);

	if (show_trigger)
	{
		// UDMF Triggers
		if (theMapEditor->currentMapDesc().format == MAP_UDMF)
		{
			// Get all UDMF properties
			vector<udmfp_t> props = theGameConfiguration->allUDMFProperties(MOBJ_LINE);
			sort(props.begin(), props.end());

			// Get all UDMF trigger properties
			vector<string> triggers;
			NamedFlexGridMap named_flexgrids;
			for (unsigned a = 0; a < props.size(); a++)
			{
				UDMFProperty* property = props[a].property;
				if (!property->isTrigger())
					continue;

				string group = property->getGroup();
				wxFlexGridSizer* frame_sizer = named_flexgrids[group];
				if (!frame_sizer)
				{
					wxStaticBox* frame_triggers = new wxStaticBox(panel_action_special, -1, group);
					wxStaticBoxSizer* sizer_triggers = new wxStaticBoxSizer(frame_triggers, wxVERTICAL);
					sizer->Add(sizer_triggers, 0, wxEXPAND|wxTOP, 4);

					frame_sizer = new wxFlexGridSizer(3);
					frame_sizer->AddGrowableCol(0, 1);
					frame_sizer->AddGrowableCol(1, 1);
					frame_sizer->AddGrowableCol(2, 1);
					sizer_triggers->Add(frame_sizer, 1, wxEXPAND|wxALL, 4);

					named_flexgrids.find(group)->second = frame_sizer;
				}

				wxCheckBox* cb_trigger = new wxCheckBox(panel_action_special, -1, property->getName(), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE);
				frame_sizer->Add(cb_trigger, 0, wxEXPAND);

				triggers.push_back(property->getName());
				triggers_udmf.push_back(property->getProperty());
				cb_triggers.push_back(cb_trigger);
			}
		}

		// Hexen trigger
		else if (theMapEditor->currentMapDesc().format == MAP_HEXEN)
		{
			// Add triggers dropdown
			wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);
			sizer->Add(hbox, 0, wxEXPAND|wxTOP, 4);

			hbox->Add(new wxStaticText(panel_action_special, -1, "Special Trigger:"), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 4);
			choice_trigger = new wxChoice(panel_action_special, -1, wxDefaultPosition, wxDefaultSize, theGameConfiguration->allSpacTriggers());
			hbox->Add(choice_trigger, 1, wxEXPAND);
		}
	}

	panel_action_special->SetSizerAndFit(sizer);
}

/* ActionSpecialPanel::setSpecial
 * Selects the item for special [special] in the specials tree
 *******************************************************************/
void ActionSpecialPanel::setSpecial(int special)
{
	// Check for boom generalised special
	if (theGameConfiguration->isBoom())
	{
		if (panel_gen_specials->loadSpecial(special))
		{
			rb_generalised->SetValue(true);
			showGeneralised(true);
			panel_gen_specials->SetFocus();
			return;
		}
		else
			rb_special->SetValue(true);
	}

	// Regular action special
	showGeneralised(false);
	tree_specials->showSpecial(special);
	tree_specials->SetFocus();
	tree_specials->SetFocusFromKbd();
	text_special->SetValue(S_FMT("%d", special));

	// Setup args if any
	if (panel_args)
	{
		argspec_t args = theGameConfiguration->actionSpecial(selectedSpecial())->getArgspec();
		panel_args->setup(&args);
	}
}

/* ActionSpecialPanel::setTrigger
 * Sets the action special trigger (hexen or udmf)
 *******************************************************************/
void ActionSpecialPanel::setTrigger(int index)
{
	if (!show_trigger)
		return;

	// UDMF Trigger
	if (cb_triggers.size() > 0)
	{
		if (index < (int)cb_triggers.size())
			cb_triggers[index]->SetValue(true);
	}

	// Hexen trigger
	else
		choice_trigger->SetSelection(index);
}

/* ActionSpecialPanel::selectedSpecial
 * Returns the currently selected action special
 *******************************************************************/
int ActionSpecialPanel::selectedSpecial()
{
	if (theGameConfiguration->isBoom())
	{
		if (rb_special->GetValue())
			return tree_specials->selectedSpecial();
		else
			return panel_gen_specials->getSpecial();
	}
	else
		return tree_specials->selectedSpecial();
}

/* ActionSpecialPanel::showGeneralised
 * If [show] is true, show the generalised special panel, otherwise
 * show the action special tree
 *******************************************************************/
void ActionSpecialPanel::showGeneralised(bool show)
{
	if (!theGameConfiguration->isBoom())
		return;

	if (show)
	{
		wxSizer* sizer = GetSizer();
		sizer->Replace(panel_action_special, panel_gen_specials);
		panel_action_special->Show(false);
		panel_gen_specials->Show(true);
		Layout();
	}
	else
	{
		wxSizer* sizer = GetSizer();
		sizer->Replace(panel_gen_specials, panel_action_special);
		panel_action_special->Show(true);
		panel_gen_specials->Show(false);
		Layout();
	}
}

/* ActionSpecialPanel::applyTo
 * Applies selected special (if [apply_special] is true), trigger(s)
 * and args (if any) to [lines]
 *******************************************************************/
void ActionSpecialPanel::applyTo(vector<MapObject*>& lines, bool apply_special)
{
	// Special
	int special = selectedSpecial();
	if (apply_special && special >= 0)
	{
		for (unsigned a = 0; a < lines.size(); a++)
			lines[a]->setIntProperty("special", special);
	}

	// Args
	if (panel_args)
	{
		// Get values
		int args[5];
		args[0] = panel_args->getArgValue(0);
		args[1] = panel_args->getArgValue(1);
		args[2] = panel_args->getArgValue(2);
		args[3] = panel_args->getArgValue(3);
		args[4] = panel_args->getArgValue(4);

		for (unsigned a = 0; a < lines.size(); a++)
		{
			if (args[0] >= 0) lines[a]->setIntProperty("arg0", args[0]);
			if (args[1] >= 0) lines[a]->setIntProperty("arg1", args[1]);
			if (args[2] >= 0) lines[a]->setIntProperty("arg2", args[2]);
			if (args[3] >= 0) lines[a]->setIntProperty("arg3", args[3]);
			if (args[4] >= 0) lines[a]->setIntProperty("arg4", args[4]);
		}
	}

	// Trigger(s)
	if (show_trigger)
	{
		for (unsigned a = 0; a < lines.size(); a++)
		{
			// UDMF
			if (!cb_triggers.empty())
			{
				for (unsigned b = 0; b < cb_triggers.size(); b++)
				{
					if (cb_triggers[b]->Get3StateValue() == wxCHK_UNDETERMINED)
						continue;

					lines[a]->setBoolProperty(triggers_udmf[b], cb_triggers[b]->GetValue());
				}
			}

			// Hexen
			else if (choice_trigger && choice_trigger->GetSelection() >= 0)
				theGameConfiguration->setLineSpacTrigger(choice_trigger->GetSelection(), (MapLine*)lines[a]);
		}
	}
}

/* ActionSpecialPanel::openLines
 * Loads special/trigger/arg values from [lines]
 *******************************************************************/
void ActionSpecialPanel::openLines(vector<MapObject*>& lines)
{
	if (lines.empty())
		return;

	// Special
	int special = lines[0]->intProperty("special");
	MapObject::multiIntProperty(lines, "special", special);
	setSpecial(special);

	// Args
	if (panel_args)
	{
		int args[5] = { -1, -1, -1, -1, -1 };
		MapObject::multiIntProperty(lines, "arg0", args[0]);
		MapObject::multiIntProperty(lines, "arg1", args[1]);
		MapObject::multiIntProperty(lines, "arg2", args[2]);
		MapObject::multiIntProperty(lines, "arg3", args[3]);
		MapObject::multiIntProperty(lines, "arg4", args[4]);
		panel_args->setValues(args);
	}

	// Trigger (UDMF)
	if (show_trigger)
	{
		if (!cb_triggers.empty())
		{
			for (unsigned a = 0; a < triggers_udmf.size(); a++)
			{
				bool set;
				if (MapObject::multiBoolProperty(lines, triggers_udmf[a], set))
					cb_triggers[a]->SetValue(set);
				else
					cb_triggers[a]->Set3StateValue(wxCHK_UNDETERMINED);
			}
		}

		// Trigger (Hexen)
		else if (choice_trigger)
		{
			int trigger = theGameConfiguration->spacTriggerIndexHexen((MapLine*)lines[0]);
			for (unsigned a = 1; a < lines.size(); a++)
			{
				if (trigger != theGameConfiguration->spacTriggerIndexHexen((MapLine*)lines[a]))
				{
					trigger = -1;
					break;
				}
			}

			if (trigger >= 0)
				choice_trigger->SetSelection(trigger);
		}
	}
}


/*******************************************************************
 * ACTIONSPECIALPANEL CLASS EVENTS
 *******************************************************************/

/* ActionSpecialPanel::onRadioButtonChanged
 * Called when the radio button selection is changed
 *******************************************************************/
void ActionSpecialPanel::onRadioButtonChanged(wxCommandEvent& e)
{
	// Swap panels
	showGeneralised(rb_generalised->GetValue());
}

/* ActionSpecialPanel::onSpecialSelectionChanged
 * Called when the action special selection is changed
 *******************************************************************/
void ActionSpecialPanel::onSpecialSelectionChanged(wxDataViewEvent &e)
{
	if ((theGameConfiguration->isBoom() && rb_generalised->GetValue()))
	{
		e.Skip();
		return;
	}

	// Set special # text box
	text_special->SetValue(S_FMT("%d", selectedSpecial()));

	if (panel_args)
	{
		argspec_t args = theGameConfiguration->actionSpecial(selectedSpecial())->getArgspec();
		// Save and restore the current arg values, since setup() deletes and
		// recreates the controls
		int arg_values[5];
		for (unsigned a = 0; a < 5; a++)
			arg_values[a] = panel_args->getArgValue(a);
		panel_args->setup(&args);
		panel_args->setValues(arg_values);
	}
}

/* ActionSpecialPanel::SpecialItemActivated
 * Called when the action special item is activated (double-clicked or
 * enter pressed)
 *******************************************************************/
void ActionSpecialPanel::onSpecialItemActivated(wxDataViewEvent &e)
{
	// Jump to args tab, if there is one
	if (panel_args)
	{
		argspec_t args = theGameConfiguration->actionSpecial(selectedSpecial())->getArgspec();
		// Save and restore the current arg values, since setup() deletes and
		// recreates the controls
		int arg_values[5];
		for (unsigned a = 0; a < 5; a++)
			arg_values[a] = panel_args->getArgValue(a);
		panel_args->setup(&args);
		panel_args->setValues(arg_values);
		panel_args->SetFocus();
	}
}

/* ActionSpecialPanel::onSpecialTextChanged
 * Called when the action special number box is changed
 *******************************************************************/
void ActionSpecialPanel::onSpecialTextChanged(wxCommandEvent& e)
{
	tree_specials->showSpecial(text_special->getNumber(), false);
}

/*******************************************************************
 * ACTIONSPECIALDIALOG CLASS FUNCTIONS
 *******************************************************************/

/* ActionSpecialDialog::ActionSpecialDialog
 * ActionSpecialDialog class constructor
 *******************************************************************/
ActionSpecialDialog::ActionSpecialDialog(wxWindow* parent, bool show_args)
: SDialog(parent, "Select Action Special", "actionspecial", 400, 500)
{
	panel_args = NULL;
	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(sizer);

	// No args
	if (theMapEditor->currentMapDesc().format == MAP_DOOM || !show_args)
	{
		panel_special = new ActionSpecialPanel(this);
		sizer->Add(panel_special, 1, wxEXPAND|wxLEFT|wxRIGHT|wxTOP, 10);
	}

	// Args (use tabs)
	else
	{
		stc_tabs = new STabCtrl(this, false);
		sizer->Add(stc_tabs, 1, wxEXPAND|wxLEFT|wxRIGHT|wxTOP, 10);

		// Special panel
		panel_special = new ActionSpecialPanel(stc_tabs);
		stc_tabs->AddPage(panel_special, "Special");

		// Args panel
		panel_args = new ArgsPanel(stc_tabs);
		stc_tabs->AddPage(panel_args, "Args");
		panel_special->setArgsPanel(panel_args);
	}

	// Add buttons
	sizer->AddSpacer(4);
	sizer->Add(CreateButtonSizer(wxOK|wxCANCEL), 0, wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM, 10);

	// Init
	SetSizerAndFit(sizer);
	CenterOnParent();
}

/* ActionSpecialDialog::~ActionSpecialDialog
 * ActionSpecialDialog class constructor
 *******************************************************************/
ActionSpecialDialog::~ActionSpecialDialog()
{
}

/* ActionSpecialDialog::setSpecial
 * Selects the item for special [special] in the specials tree
 *******************************************************************/
void ActionSpecialDialog::setSpecial(int special)
{
	panel_special->setSpecial(special);
	if (panel_args)
	{
		argspec_t args = theGameConfiguration->actionSpecial(special)->getArgspec();
		panel_args->setup(&args);
	}
}

/* ActionSpecialDialog::setArgs
 * Sets the arg values
 *******************************************************************/
void ActionSpecialDialog::setArgs(int args[5])
{
	if (panel_args)
		panel_args->setValues(args);
}

/* ActionSpecialDialog::selectedSpecial
 * Returns the currently selected action special
 *******************************************************************/
int ActionSpecialDialog::selectedSpecial()
{
	return panel_special->selectedSpecial();
}

/* ActionSpecialDialog::getArg
 * Returns the value of arg [index]
 *******************************************************************/
int ActionSpecialDialog::getArg(int index)
{
	if (panel_args)
		return panel_args->getArgValue(index);
	else
		return 0;
}

/* ActionSpecialDialog::applyTriggers
 * Applies selected trigger(s) (hexen or udmf) to [lines]
 *******************************************************************/
void ActionSpecialDialog::applyTo(vector<MapObject*>& lines, bool apply_special)
{
	panel_special->applyTo(lines, apply_special);
}

/* ActionSpecialDialog::openLines
 * Loads special/trigger/arg values from [lines]
 *******************************************************************/
void ActionSpecialDialog::openLines(vector<MapObject*>& lines)
{
	panel_special->openLines(lines);
}
