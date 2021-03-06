/*	主窗口显示模块
 *	负责显示当前用户输入上下文内容。
 *
 */
#include <time.h>
#include <windows.h>
#include <gdiplus.h>

using namespace Gdiplus;

#include <assert.h>
#include <config.h>
#include <win32/main_window.h>
#include <win32/pim_ime.h>
#include <win32/softkbd.h>
#include <utility.h>
#include <editor.h>
#include <zi.h>
#include <ci.h>
#include <resource.h>
#include <tchar.h>
//#include <url.h>
#include <share_segment.h>
#include <libfunc.h>
#include <fontcheck.h>

#define	TESTCONTEXTS		if (!context || !ui_context) return
#define	HINT_TEXT_GAP		6
#define	HINT_LINE_GAP		2

static int		global_draw_assist_line;						//是否绘制辅助线
static int		status_button_width, status_button_height;		//状态窗口按钮的大小
static int		status_window_width, status_window_height;		//状态窗口的大小
static int		status_buttons = 0;								//上一次的按钮配置
static int		status_button_count;							//状态窗口按钮数目
static int		status_button_id[MAX_STATUS_BUTTON_NUMBER];		//状态窗口按钮标志
static POINT	status_button_pos[MAX_STATUS_BUTTON_NUMBER];	//状态窗口按钮位置
static int		theme_index;									//当前鼠标选择的主题索引
static int		theme_previewing = 0;							//右键主题预览中
static int		theme_demo_previewing = 0;						//主题预览中
static int      isSmooth = 0;

//#pragma data_seg(HYPIM_SHARED_SEGMENT)
//HWND main_window_list[MAX_STATUS_LIST_SIZE]		= {0,};
//HWND status_window_list[MAX_STATUS_LIST_SIZE]	= {0,};
//HWND hint_window_list[MAX_STATUS_LIST_SIZE]		= {0,};
//#pragma data_seg()

PIMCONFIG *pim_config_save, pim_config_theme;

UICONTEXT default_ui_context =
{
	0,											//Context指针

	0,											//输入法消息窗口
	0,											//写作窗口
	0,											//状态窗口
	0,											//提示窗口
	0,											//ToolTip窗口
	0,											//main预览窗口
	0,											//status预览窗口

	{POS_DEFAULT_X, POS_DEFAULT_Y},				//写作窗口的位置
	{0, 0,},									//写作窗口的大小
	{0, 0,},									//提示窗口的大小
	0,											//写作部分的高度
	0, 											//候选行高度

	0,											//是否已经有光标位置信息，
	{POS_DEFAULT_X, POS_DEFAULT_Y},				//光标位置

	0,											//是否正在显示菜单
	0,											//处理状态条的显示或隐藏

	0,											//中文字体
	0,											//英文字体
	0,											//数字字体
	0,											//音调字体
	0,											//提示字体
	0,                                          //功能提示字体

	0,											//写作窗口背景图像
	0,											//写作窗口中心图像
	0,											//写作窗口竖排背景图像
	0,											//写作窗口竖排中心图像
	0,											//状态窗口背景图像
	0,											//状态窗口按钮图像
};

static int show_cursor = 1;					//当前光标显示状态

typedef struct tagSTATUS_BUTTON_HINT
{
	const int   item_type;
	const TCHAR item_text[MAX_HINT_LENGTH];
}STATUS_BUTTON_HINT_ITEM;

static STATUS_BUTTON_HINT_ITEM status_button_hint[] =
{
	STATUS_BUTTON_HINT_INPUT_MODE,		TEXT("中文/英文输入"),
	STATUS_BUTTON_HINT_OUTPUT_MODE,		TEXT("简体/繁体/全集(ujt/uft/uqj)"),
	STATUS_BUTTON_HINT_MARK_MODE,		TEXT("中文标点/英文标点模式(Ctrl+.)"),
	STATUS_BUTTON_HINT_SHAP_MODE,		TEXT("全/半角模式(Shift+Space)"),
	STATUS_BUTTON_HINT_SOFTKEYBOARD,	TEXT("软键盘开/关"),
	STATUS_BUTTON_HINT_SETUP,			TEXT("设置程序(usetup)"),
	STATUS_BUTTON_HINT_HELP,			TEXT("帮助"),
};

static TCHAR hint_string[MAX_HINT_LENGTH] = TEXT("Hint Test String");		//提示信息

static int current_down_button_index = -1;									//当前鼠标按下的按钮索引
static int current_move_button_index = -1;									//当前鼠标悬停的按钮索引

//候选列宽度
static int 	candidate_rows, candidate_cols;									//候选行数与列数
static int	max_candidate_width = 0;										//最大候选宽度
static int	candidate_col_width[MAX_CANDIDATES_PER_LINE];					//候选每列的宽度
static RECT	candidate_rect[MAX_CANDIDATE_LINES * MAX_CANDIDATES_PER_LINE];	//每一个候选的位置，用于进行鼠标点击判断

typedef struct tagCAND_MENU_ITEM
{
	const int	item_type;
	const TCHAR *item_caption;
}CAND_MENU_ITEM;

static CAND_MENU_ITEM candidate_menu_item[] =
{
	CAND_POPUP_MENU_SET_TOP,	TEXT("置顶:"),
	CAND_POPUP_MENU_DELETE,		TEXT("删除:"),
	CAND_POPUP_MENU_BAIDU,		TEXT("搜索:"),
};

static TCHAR search_prefix_baidu[MAX_SEARCH_URL_LENGTH] = TEXT("http://www.baidu.com/baidu?tn=sitezg_dg&word=");
static TCHAR search_prefix_iciba[MAX_SEARCH_URL_LENGTH] = TEXT("http://www.iciba.com/search?s=%s&site=ziguang");

typedef struct tagCAND_HINT_ITEM
{
	const int	hint_type;
	const TCHAR	*hint_string;
}CAND_HINT_ITEM;

static CAND_HINT_ITEM compose_hint_item[] =
{
	CAND_HINT_TYPE_ENGLISH_CN,		TEXT("逗号进入英文候选"),
	CAND_HINT_TYPE_ENGLISH_EN,		TEXT("逗号退出英文候选"),
	CAND_HINT_TYPE_ENGLISH_INPUT,   TEXT("英文输入法"),
	CAND_HINT_TYPE_ABC,				TEXT("按空格出字"),
	CAND_HINT_TYPE_USE_TAB,			TEXT("使用Tab键可以显示更多候选"),
	CAND_HINT_TYPE_NUMBER_STRING,	TEXT("i 加数字可快速输入中文数字"),
	CAND_HINT_TYPE_U_CHAR,          TEXT("命令直通车 空格执行"),
	CAND_HINT_TYPE_AT_CHAR,			TEXT("再次使用Shift+2可输入@"),
};

static TCHAR CAND_HINT_ICIBA[MAX_HINT_LENGTH]	= TEXT(" [F9]专业翻译");
static TCHAR CAND_HINT_DEFAULT[MAX_HINT_LENGTH] = TEXT(" [F9]搜索");
static TCHAR hint_message[MAX_CAND_HINT_SIZE]	= TEXT("");					//智能提示信息

void RegisterMainWindow(HWND status_handle, HWND handle)
{
	for (int i = 0; i < MAX_STATUS_LIST_SIZE; i++)
	{
		if (status_handle == (HWND)share_segment->status_window_list[i])
		{
			share_segment->main_window_list[i] = (unsigned __int64)handle;
			break;
		}
	}
}

void RegisterStatusWindow(HWND handle)
{
	for (int i = 0; i < MAX_STATUS_LIST_SIZE; i++)
	{
		if (share_segment->status_window_list[i] != 0)
			continue;

		share_segment->status_window_list[i] = (unsigned __int64)handle;
		break;
	}
}

void RegisterHintWindow(HWND status_handle, HWND handle)
{
	for (int i = 0; i < MAX_STATUS_LIST_SIZE; i++)
	{
		if (status_handle == (HWND)share_segment->status_window_list[i])
		{
			share_segment->hint_window_list[i] = (unsigned __int64)handle;
			break;
		}
	}
}

void UnregisterStatusWindow(HWND handle)
{
	for (int i = 0; i < MAX_STATUS_LIST_SIZE; i++)
	{
		if (handle == (HWND)share_segment->status_window_list[i])
		{
			share_segment->status_window_list[i] = 0;
			break;
		}
	}
}

//装载外部随机提示信息
int LoadRandomHintMessage()
{
 //   typedef int (_stdcall *pGetHintMessage) (TCHAR *, int);

	//HMODULE dll;
	//pGetHintMessage getHintMessage;
	//int ret;

	if (!pim_config->show_hint_msg)
	{
		if (hint_message[0])
			hint_message[0] = 0;

		return 0;
	}

	if (GetTickCount() % 10 != 0)
	{
		if (hint_message[0])
			hint_message[0] = 0;

		return 0;
	}

	//Log(LOG_ID, L"装载DLL");
	//dll = LoadLibrary(UTILITY_DLL_NAME);
	//if (!dll)
	//	return 0;

	//ret = 0;
	//getHintMessage = (pGetHintMessage) GetProcAddress(dll, "GetHintMessage");
	//if (getHintMessage)
	//	ret = (*getHintMessage)(hint_message, MAX_CAND_HINT_SIZE);

	//Log(LOG_ID, L"调用GetHintMessage");

	//FreeLibrary(dll);

	//Log(LOG_ID, L"卸载DLL");

	return GetHintMessage(hint_message, MAX_CAND_HINT_SIZE);
}

/**	获得UI窗口句柄，在窗口的LONG中
 */
HWND GetUIWindowHandle(HWND hwnd)
{
	return (HWND)(LONG_PTR)GetWindowLongPtr(hwnd, GWLP_USERDATA);	//通过USERDATA获得UI窗口句柄
}

/**	获得UIContext指针
 */
UICONTEXT *GetUIContextByWindow(HWND ui_window)
{
	//需要判断ui_context的合法性（TestManager出错，Release版本没有这个问题，奇怪）
	if (!IsWindow(ui_window))
		return 0;

	return (UICONTEXT*)(LONG_PTR)GetWindowLongPtr(ui_window, IMMGWLP_PRIVATE);
}

void GetStringWidth(PIMCONTEXT *context, UICONTEXT *ui_context, const TCHAR* pWChar, int nLen, Graphics &g, int *width, int *height,Font *aFont, int is_compose_str)
{
	const TCHAR *p, *q;
	Font *pf = NULL;
	TCHAR demo_string[] = TEXT("我们");
	int old_len = nLen;
	*width = *height = 0;
	if (nLen <= 0 )
	{
		nLen   = (int)_tcslen(demo_string);
		pWChar = demo_string;
	}

	Region* pCharRangeRegions;

	RectF layoutRect(0, 0, 4000, 4000);
	Rect R;

	StringFormat strFormat;

	pf = aFont;
	p  = pWChar;
	while (p < pWChar + nLen)
	{
		if (_IsNoneASCII(*p))
		{
			q = p + _HanZiLen;
			while (q < pWChar + nLen && (_IsNoneASCII(*q)))
				q += _HanZiLen;

			if (!aFont)
				pf = ui_context->zi_font;
		}
		else					//英文，寻找连续英文
		{
			//正确的音调输入，显示音调
			if (is_compose_str && context->syllable_count && *p >= '1' && *p <= '4')
			{
				q = p + 1;
				if (!aFont)
					pf = ui_context->tone_font;
			}
			else
			{
				q = p + 1;
				while(q < pWChar + nLen && (!_IsNoneASCII(*q)) &&
					 ((*q < '1' || *q > '4') || (*q >= '1' && *q <= '4' && !context->syllable_count)))
					q++;
				if (!aFont)
					pf = ui_context->ascii_font;
			}
		}

		CharacterRange charRanges(0, (int)(q - p));
		strFormat.SetMeasurableCharacterRanges(1, &charRanges);
		strFormat.SetFormatFlags(StringFormatFlagsMeasureTrailingSpaces);

		int count = strFormat.GetMeasurableCharacterRangeCount();
		pCharRangeRegions = new Region[count];

		//HDC dc = g.GetHDC();
		//Font font(dc, pf);
		//g.ReleaseHDC(dc);

		// Get the regions that correspond to the ranges within the string.
		g.MeasureCharacterRanges(p, (int)(q - p), pf, layoutRect, &strFormat, 1, pCharRangeRegions);
		Status s = pCharRangeRegions->GetBounds(&R, &g);
		*width += (int)R.Width;
		if (is_compose_str)
			*width += (int)R.X;
		if ((int)R.Height > *height)
		  *height =	(int)R.Height;
		p = q;
	}
	if (!old_len)
		*width = 0;
}

void GetHintString(PIMCONTEXT *context, TCHAR *hint_text)
{
	int i, hint_type = 0, nRemains;

	if (theme_previewing)
		return;
	hint_text[0] = 0;
	nRemains = context->candidate_count - context->candidate_index - context->candidate_page_count;
	if (context->candidate_count){
		for(i=0;i<pim_config->candidates_per_line;i++){
			if(context->candidate_array[i].type == CAND_TYPE_SPW &&
				(SPW_STRING_NORMAL == context->candidate_array[i].spw.type) && context->candidate_array[i].spw.hint)
			{
				//当助记符不是单短语位置信息时，则输出hint
				//判断是否是位置信息，用：[1]~[9]以外的都不是
				if(!(_tcslen((TCHAR *)context->candidate_array[i].spw.hint) == 3 
					&& ((TCHAR *)context->candidate_array[i].spw.hint)[1] > TEXT('0') 
					&& ((TCHAR *)context->candidate_array[i].spw.hint)[1] <= TEXT('9'))){
					_tcscpy_s(hint_text, MAX_CAND_HINT_SIZE, (TCHAR *)context->candidate_array[i].spw.hint);
					return;
				}
			}
		}
	}
	//U模式
	if (context->state == STATE_UINPUT && context->english_state == ENGLISH_STATE_NONE && pim_config->u_mode_enabled)
		hint_type = CAND_HINT_TYPE_U_CHAR;
	//abc模式 提示空格出字
	else if (STYLE_ABC == pim_config->input_style && context->state != STATE_ABC_SELECT &&
		context->state != STATE_VINPUT && context->english_state != ENGLISH_STATE_INPUT)
		hint_type = CAND_HINT_TYPE_ABC;
	else if (context->english_state == ENGLISH_STATE_INPUT)
		hint_type = CAND_HINT_TYPE_ENGLISH_INPUT;
	else if (context->english_state == ENGLISH_STATE_CAND && context->candidate_index <= 0)
		hint_type = CAND_HINT_TYPE_ENGLISH_EN;
	else if (context->has_english_candidate && context->candidate_index <= 0 && context->input_length > 2)
		hint_type = CAND_HINT_TYPE_ENGLISH_CN;
	//翻页超过2页 and 后面还有超过1页 and 能够Tab切换  提示tab扩展显示
	else if (context->candidate_index >= (context->candidate_page_count * 2) && 
			 (nRemains > pim_config->candidates_per_line) && CanSwitchToExpandMode(context))
		hint_type = CAND_HINT_TYPE_USE_TAB;
	//有候选 and 第一个候选是数字串
	else if ((context->candidate_page_count > 0) && IsNumberString(context->candidate_string[0]) &&
			 (PINYIN_QUANPIN == pim_config->pinyin_mode))
		hint_type = CAND_HINT_TYPE_NUMBER_STRING;
	//提示输入@
	else if (!pim_config->use_hz_tone && LastCharIsAtChar(context->input_string))
		hint_type = CAND_HINT_TYPE_AT_CHAR;
	else
		hint_type = CAND_HINT_TYPE_RANDOM;

    if (theme_demo_previewing)
		hint_type = CAND_HINT_TYPE_RANDOM;

	if (CAND_HINT_TYPE_RANDOM == hint_type/* && pim_config->show_hint_msg*/)
	{
		if (!_tcslen(hint_message) && context->candidate_count)
		{
			TCHAR string[MAX_COMPOSE_LENGTH + 4];
			int cand_index = context->candidate_selected_index + context->candidate_index;
			int	length = (int)_tcslen(CAND_HINT_DEFAULT);
			_tcscpy_s(hint_text, MAX_CAND_HINT_SIZE, CAND_HINT_DEFAULT);
			GetCandidateDisplayString(context, &context->candidate_array[cand_index], string, MAX_COMPOSE_LENGTH / 2, 0);
			if (_tcslen(string) <= 6)
			{
				_tcscat_s(hint_text, MAX_CAND_HINT_SIZE - length, TEXT(":"));
				_tcscat_s(hint_text, MAX_CAND_HINT_SIZE - length - 1, string);
			}
			return;
		}
		_tcscpy_s(hint_text, MAX_CAND_HINT_SIZE, hint_message);
		return;
	}
	for (i = 0; i < _SizeOf(compose_hint_item); i++)
	{
		if (hint_type == compose_hint_item[i].hint_type)
		{
			_tcscpy_s(hint_text, MAX_CAND_HINT_SIZE, compose_hint_item[i].hint_string);
			if (pim_config->use_english_translate && (hint_type == CAND_HINT_TYPE_ENGLISH_INPUT || hint_type == CAND_HINT_TYPE_ENGLISH_EN))
				_tcscat_s(hint_text, MAX_CAND_HINT_SIZE - _tcslen(hint_text), CAND_HINT_ICIBA);
			break;
		}
	}
}

//是否使用竖排背景
int use_vertical_background(PIMCONTEXT *context, UICONTEXT *ui_context, PIMCONFIG *config)
{
	if (!ui_context->image_main_vert_bk)
		return 0;
	switch (context->candidates_view_mode)
	{
	case VIEW_MODE_VERTICAL:
		return 1;
	case VIEW_MODE_EXPAND:
		if (config->use_vert_bk_when_expanding)
			return 1;
	}
	return 0;
}

/**	中文之星输入风格
 */
static void PreShowMainWindowCStar(PIMCONTEXT *context, UICONTEXT *ui_context, HDC dc)
{
	//Font *aFont = NULL;
	int i, j;
	int width, height, max_help_width = 0;
	int	compose_width, compose_height;				//写作窗口高度与宽度
	int candidate_line_width;						//候选行宽度
	int no_width, no_height;						//写作窗口右侧指示数字
	int hint_width, hint_height;					//提示信息高度与宽度
	int candidate_count, candidates_per_line;
	TCHAR string[MAX_CANDIDATE_STRING_LENGTH + 4];
	TCHAR help_string[MAX_TRANSLATE_STRING_LENGTH];
	TCHAR hint_string[MAX_CAND_HINT_SIZE] = {0};
	int st = clock();
	Graphics g(dc);

	memset(candidate_col_width, 0, sizeof(candidate_col_width));
	memset(candidate_rect, 0, sizeof(candidate_rect));

	max_candidate_width = 0;

	if (!dc)
	{
		ui_context->main_window_size.cx = ui_context->main_window_size.cy = 0;
		return;
	}

	//此处TCHAR的转换需要测试
	//1. 确定Composition长度
	GetStringWidth(context,  
				   ui_context, 
				   context->state == STATE_IEDIT ? (TCHAR*)context->iedit_hz : context->compose_string,
				   context->state == STATE_IEDIT ? (int)_tcslen((TCHAR*)context->iedit_hz) : (int)_tcslen(context->compose_string),
				   g,
				   &compose_width,
				   &compose_height,
				   0,
				   1);

	int main_text_left_margin, main_text_right_margin, main_text_top_margin, main_text_bottom_margin;
	int main_remain_number_margin, main_center_gap_height;
	int main_window_min_width, main_window_min_height, hint_right_margin;
	int use_vertical_bk = use_vertical_background(context, ui_context, pim_config);

	main_text_left_margin	  = use_vertical_bk ? pim_config->main_vert_text_left_margin : pim_config->main_text_left_margin;
	main_text_right_margin	  = use_vertical_bk ? pim_config->main_vert_text_right_margin : pim_config->main_text_right_margin;
	main_text_top_margin	  = use_vertical_bk ? pim_config->main_vert_text_top_margin : pim_config->main_text_top_margin;
	main_text_bottom_margin	  = use_vertical_bk ? pim_config->main_vert_text_bottom_margin : pim_config->main_text_bottom_margin;
	main_remain_number_margin = use_vertical_bk ? pim_config->main_vert_remain_number_margin : pim_config->main_remain_number_margin;
	main_center_gap_height	  = use_vertical_bk ? pim_config->main_vert_center_gap_height : pim_config->main_center_gap_height;
	main_window_min_width	  = use_vertical_bk ? pim_config->main_window_vert_min_width : pim_config->main_window_min_width;
	main_window_min_height	  = use_vertical_bk ? pim_config->main_window_vert_min_height : pim_config->main_window_min_height;
	hint_right_margin		  = use_vertical_bk ? pim_config->hint_vert_right_margin : pim_config->hint_right_margin;

	//获取提示串
	if(pim_config->show_hint_msg)
		GetHintString(context, hint_string);
	else
		hint_string[0]=0;

	if (hint_string[0] && (hint_right_margin >= 0))
	{
		GetStringWidth(context, ui_context, hint_string, (int)_tcslen(hint_string), g, &hint_width, &hint_height, 0, 0);
		compose_width += hint_width;
	}
	//增加指示候选个数的显示
	GetStringWidth(context, ui_context, TEXT("99999"), 5, g, &no_width, &no_height, 0, 0);
	if (main_remain_number_margin >= 0)
		compose_width += no_width;

	if (!hint_right_margin || (hint_right_margin - main_remain_number_margin < no_width))
	{
		hint_right_margin = main_remain_number_margin + no_width + MIN_HINT_NO_GAP;

		if (use_vertical_bk)
			pim_config->hint_vert_right_margin = hint_right_margin;
		else
			pim_config->hint_right_margin = hint_right_margin;
	}

	//双拼提示串宽度
	if (pim_config->pinyin_mode == PINYIN_SHUANGPIN && pim_config->show_sp_hint)
	{
		TCHAR str[0x100] = TEXT("[");
		int  w, h;
		_tcscat_s(str, _SizeOf(str), context->sp_hint_string);
		_tcscat_s(str, TEXT("]"));
		GetStringWidth(context, ui_context, str, (int)_tcslen(str), g, &w, &h, 0, 0);
		compose_width += w;
	}

	ui_context->compose_frame_height = compose_height;			//全局的写作窗口高度

	//2. 确定candidate的宽度与高度
	//(No)Ci(Gap)(No)Ci(Gap)(No)....Ci(Gap)(No)Ci
	//在有候选或者没有spw提示的情况下进行下列操作
	if (context->candidate_count || !context->spw_hint_string[0])
	{
		if (pim_config->candidates_per_line == 0)			//注意除以0错误
			pim_config->candidates_per_line = 5;

		candidates_per_line = pim_config->candidates_per_line;

		switch (context->candidates_view_mode)
		{
		case VIEW_MODE_VERTICAL:	//竖排显示
			candidate_count		= min(context->candidate_page_count, candidates_per_line);
			candidate_rows		= candidate_count;
			candidates_per_line = 1;
			break;

		case VIEW_MODE_EXPAND:		//扩展显示
			candidate_count = min(context->candidate_page_count, candidates_per_line * GetExpandCandidateLine());
			candidate_rows  = (candidate_count + candidates_per_line - 1) / candidates_per_line;
			break;

		default:					//横排显示
			candidate_count	= min(context->candidate_page_count, candidates_per_line);
			candidate_rows  = (candidate_count + candidates_per_line - 1) / candidates_per_line;

			break;
		}

		candidate_line_width = ui_context->candidate_line_height = 0;
		//找出每一个候选的宽度，用于进行排列的计算
		for (i = 0; i < candidate_count; i++)
		{
			int row = i / candidates_per_line;
			int col = i % candidates_per_line;
			help_string[0] = 0;
			if (VIEW_MODE_EXPAND == context->candidates_view_mode)
				_stprintf_s(string, _SizeOf(string), TEXT("%1d%1d%s"), row + 1, col + 1, context->candidate_string[i]);
			else
				_stprintf_s(string, _SizeOf(string), TEXT("%1d%s"), i + 1, context->candidate_string[i]);

			if (context->english_state != ENGLISH_STATE_NONE && pim_config->use_english_input && pim_config->use_english_translate)
			{
				if (context->candidate_trans_string[i][0])
				{
					_tcscat_s(help_string, _SizeOf(help_string), context->candidate_trans_string[i]);
					_tcscat_s(help_string, _SizeOf(help_string), TEXT(" []"));
				}
			}
			else if (context->state == STATE_UINPUT && pim_config->u_mode_enabled && pim_config->use_u_hint && (TCHAR *)context->candidate_array[i + context->candidate_index].spw.hint)
			{
				_tcscat_s(help_string, _SizeOf(help_string), (TCHAR *)context->candidate_array[i + context->candidate_index].spw.hint);
				_tcscat_s(help_string, _SizeOf(help_string), TEXT(" []"));
			}
			/*else if (context->url_state && context->candidate_array[i + context->candidate_index].url.hint)
			{
				_tcscat_s(help_string, _SizeOf(help_string), (TCHAR *)context->candidate_array[i + context->candidate_index].url.hint);
				_tcscat_s(help_string, _SizeOf(help_string), TEXT(" []"));
			}*/

			GetStringWidth(context,  ui_context, string, (int)_tcslen(string), g, &width, &height, 0, 0);
			candidate_rect[i].top = width;			//暂时使用top记录真实宽度

			//列宽度
			width += CAND_NUMBER_GAP_WIDTH + CAND_GAP_WIDTH;
			if (width > candidate_col_width[col])
			{
				candidate_line_width	+= width - candidate_col_width[col];
				candidate_col_width[col] = width;
			}

			if (width > max_candidate_width)
				max_candidate_width = width;

			//最高的列
			if (ui_context->candidate_line_height < height)
				ui_context->candidate_line_height = height;

			if (help_string[0])
			{
				//SIZE size;
				int size_cx, size_cy;
				//SelectObject(dc, ui_context->zi_font);
				GetStringWidth(context,  ui_context, help_string, (int)_tcslen(help_string), g, &size_cx, &size_cy, ui_context->zi_font, 0);
				//GetTextExtentPoint32(dc, help_string, (int)_tcslen(help_string), &size);
				if (max_help_width < size_cx)
					max_help_width = size_cx;
			}
		}

		//整理候选的矩形
		for (i = 0; i < candidate_count; i++)
		{
			int row = i / candidates_per_line;
			int col = i % candidates_per_line;

			candidate_rect[i].left = main_text_left_margin;
			for (j = 0; j < col; j++)
				candidate_rect[i].left += candidate_col_width[j];

			candidate_rect[i].right  = candidate_rect[i].left + candidate_rect[i].top;		//top中放置着宽度
			candidate_rect[i].top	 = ui_context->compose_frame_height + main_text_top_margin + main_center_gap_height + CAND_GAP_HEIGHT * row + ui_context->candidate_line_height * row;
			candidate_rect[i].bottom = candidate_rect[i].top + ui_context->candidate_line_height;
		}
	}
	else
	{	//进行spw提示串的计算, 获得字符串宽度

		GetStringWidth(context, ui_context, context->spw_hint_string, (int)_tcslen(context->spw_hint_string), g, &width, &height, 0, 0);
		candidate_line_width			  = width + 2 * CAND_GAP_WIDTH;
		ui_context->candidate_line_height = height;
		candidate_rect[0].left			  = main_text_left_margin;
		candidate_rect[0].right			  = candidate_rect[0].left + width;
		candidate_rect[0].top			  = ui_context->compose_frame_height + main_text_top_margin + main_center_gap_height;
		candidate_rect[0].bottom		  = candidate_rect[0].top + ui_context->candidate_line_height;
		candidate_rows					  = 1;
		candidate_cols					  = 0;
	}

	//设定主窗口的大小
	candidate_line_width += max_help_width;

//	if (VIEW_MODE_VERTICAL != context->candidates_view_mode)
//		candidate_line_width -= CAND_GAP_WIDTH;		//减去最后一个缝隙

	ui_context->main_window_size.cx = compose_width > candidate_line_width ? compose_width : candidate_line_width;

	if ((main_remain_number_margin >= 0) &&
		(ui_context->main_window_size.cx - main_remain_number_margin < compose_width + MIN_COMPOSE_NO_GAP - no_width))
		ui_context->main_window_size.cx += MIN_COMPOSE_NO_GAP;

	if (main_remain_number_margin > MIN_COMPOSE_NO_GAP)
		ui_context->main_window_size.cx += MIN_COMPOSE_NO_GAP;

	if (hint_right_margin >= 0)
	{
		if (ui_context->main_window_size.cx - hint_right_margin < compose_width + MIN_COMPOSE_HINT_GAP - hint_width)
			ui_context->main_window_size.cx += MIN_COMPOSE_HINT_GAP;

		if ((hint_right_margin > main_remain_number_margin) && (main_remain_number_margin >= 0))
			ui_context->main_window_size.cx += hint_right_margin - main_remain_number_margin;
	}

	if (max_candidate_width)
		max_candidate_width += main_text_left_margin;

	ui_context->main_window_size.cx += main_text_left_margin + main_text_right_margin;
	ui_context->main_window_size.cy =
		main_text_top_margin +											//文字上边界
		main_text_bottom_margin +										//文字下边界
		compose_height +												//写作框
		(candidate_rows ? main_center_gap_height : 0) +					//写作、候选间隙
		candidate_rows * ui_context->candidate_line_height +			//候选高度
		(candidate_rows ? (candidate_rows - 1) * CAND_GAP_HEIGHT : 0);	//候选行间隙

	if (ui_context->main_window_size.cx < main_window_min_width)
		ui_context->main_window_size.cx = main_window_min_width;

	if (ui_context->main_window_size.cy < main_window_min_height)
		ui_context->main_window_size.cy = main_window_min_height;

	if (ui_context->main_window_size.cx < MIN_MAIN_WINDOW_WIDTH)
		ui_context->main_window_size.cx = MIN_MAIN_WINDOW_WIDTH;

	Log(LOG_ID, L"Cost:%d", clock() - st);
}

/**	智能ABC风格
 */
static void PreShowMainWindowABC(PIMCONTEXT *context, UICONTEXT *ui_context, HDC dc)
{
	//HFONT hFont=0;
	int i;
	int width, height, no_width, no_height, max_help_width = 0;
	int	compose_width, compose_height;				//写作窗口高度与宽度
	int candidate_width, candidate_height;			//候选窗口高度与宽度
	int candidate_line_width;						//候选行宽度
	int candidate_count;
	int candidate_offset = 0;						//候选的偏移（ABC风格的对齐要求）
	TCHAR string[MAX_CANDIDATE_STRING_LENGTH + 4];
	TCHAR help_string[MAX_TRANSLATE_STRING_LENGTH];
	TCHAR hint_string[MAX_CAND_HINT_SIZE] = {0};
	int hint_width, hint_height;					//提示信息高度与宽度
	TCHAR *no_string;								//数字字符串
	Graphics g(dc);

	memset(candidate_col_width, 0, sizeof(candidate_col_width));
	memset(candidate_rect, 0, sizeof(candidate_rect));

	max_candidate_width = 0;

	if (!dc)
	{
		ui_context->main_window_size.cx = ui_context->main_window_size.cy = 0;
		return;
	}

	//1. 确定Composition宽度与高度
	if (context->state != STATE_ABC_SELECT)
	{
		GetStringWidth(context, ui_context, context->compose_string, context->compose_length, g, &compose_width, &compose_height, 0, 0);
	}
	else
	{	//处于选择状态
		TCHAR *selected_string;
		TCHAR *cand_string;
		TCHAR *remain_string;
		//SIZE size;
		int  w1, w2, h2, w3;
		int cx, cy;

		selected_string = GetSelectedComposeString(context);
		cand_string		= GetCurrentCandidateString(context);
		remain_string	= GetReaminComposeString(context);

		//SelectObject(dc, ui_context->zi_font);
		//GetTextExtentPoint32(dc, selected_string, (int)_tcslen(selected_string), &size);

		GetStringWidth(context, ui_context, selected_string, (int)_tcslen(selected_string), g, &cx, &cy, ui_context->zi_font, 1);
		w1 = cx;

		//GetTextExtentPoint32(dc, cand_string, (int)_tcslen(cand_string), &size);
		GetStringWidth(context, ui_context, cand_string, (int)_tcslen(cand_string), g, &cx, &cy, ui_context->zi_font, 1);
		w2 = cx;
		h2 = cy;

		//GetTextExtentPoint32(dc, remain_string, (int)_tcslen(remain_string), &size);
		GetStringWidth(context, ui_context, remain_string, (int)_tcslen(remain_string), g, &cx, &cy, ui_context->zi_font, 1);
		w3 = cx;

		candidate_offset = w1;
		compose_width	 = w1 + w2 + w3;
		compose_height	 = h2;
	}

	//获取候选序号的宽度
	if (pim_config->input_style == STYLE_ABC && pim_config->select_sytle == SELECTOR_LETTER &&
		context->english_state == ENGLISH_STATE_NONE)
		no_string = TEXT("a");
	else
		no_string = TEXT("1");

	GetStringWidth(context, ui_context, no_string, (int)_tcslen(no_string), g, &no_width, &no_height, 0, 0);
	compose_width += no_width;

	int main_left_margin, main_right_margin, main_top_margin, main_bottom_margin;
	int main_text_left_margin, main_text_right_margin, main_text_top_margin, main_text_bottom_margin;
	int main_center_gap_height;
	int main_window_min_width, main_window_min_height;
	int main_remain_number_margin, hint_right_margin;
	int use_vertical_bk = use_vertical_background(context, ui_context, pim_config);

	main_left_margin		  = use_vertical_bk ? pim_config->main_vert_left_margin : pim_config->main_left_margin;
	main_right_margin		  = use_vertical_bk ? pim_config->main_vert_right_margin : pim_config->main_right_margin;
	main_top_margin			  = use_vertical_bk ? pim_config->main_vert_top_margin : pim_config->main_top_margin;
	main_bottom_margin		  = use_vertical_bk ? pim_config->main_vert_bottom_margin : pim_config->main_bottom_margin;
	main_text_left_margin	  = use_vertical_bk ? pim_config->main_vert_text_left_margin : pim_config->main_text_left_margin;
	main_text_right_margin	  = use_vertical_bk ? pim_config->main_vert_text_right_margin : pim_config->main_text_right_margin;
	main_text_top_margin	  = use_vertical_bk ? pim_config->main_vert_text_top_margin : pim_config->main_text_top_margin;
	main_text_bottom_margin	  = use_vertical_bk ? pim_config->main_vert_text_bottom_margin : pim_config->main_text_bottom_margin;
	main_center_gap_height	  = use_vertical_bk ? pim_config->main_vert_center_gap_height : pim_config->main_center_gap_height;
	main_window_min_width	  = use_vertical_bk ? pim_config->main_window_vert_min_width : pim_config->main_window_min_width;
	main_window_min_height	  = use_vertical_bk ? pim_config->main_window_vert_min_height : pim_config->main_window_min_height;
	main_remain_number_margin = use_vertical_bk ? pim_config->main_vert_remain_number_margin : pim_config->main_remain_number_margin;
	hint_right_margin		  = use_vertical_bk ? pim_config->hint_vert_right_margin : pim_config->hint_right_margin;

	//获取提示串
	if(pim_config->show_hint_msg)
		GetHintString(context, hint_string);
	else
		hint_string[0]=0;

	if (hint_string[0] && (hint_right_margin >= 0))
	{
		GetStringWidth(context, ui_context, hint_string, (int)_tcslen(hint_string), g, &hint_width, &hint_height, 0, 0);
		compose_width +=hint_width;
	}

	//增加指示候选个数的显示
	GetStringWidth(context, ui_context, TEXT("999/999"), (int)_tcslen(TEXT("999/999")), g, &no_width, &no_height, 0, 0);
	if (main_remain_number_margin >= 0)
		compose_width += no_width;

	//双拼提示串宽度
	if (pim_config->pinyin_mode == PINYIN_SHUANGPIN && pim_config->show_sp_hint &&
		context->state != STATE_ABC_SELECT && context->state != STATE_ILLEGAL)
	{
		TCHAR str[0x100] = TEXT("[");
		int w, h;
		_tcscat_s(str, _SizeOf(str), context->sp_hint_string);
		_tcscat_s(str, TEXT("]"));
		GetStringWidth(context, ui_context, str, (int)_tcslen(str), g, &w, &h, 0, 0);
		compose_width += w;
	}

	ui_context->compose_frame_height = compose_height;			//全局的写作窗口高度

	//2. 确定candidate的宽度与高度
	//(No)Ci(Gap)(No)Ci(Gap)(No)....Ci(Gap)(No)Ci
	//如果处于ABC的选择候选状态
	//在有候选或者没有spw提示的情况下进行下列操作
	if (context->candidate_count || !context->spw_hint_string[0])
	{
		candidate_width = candidate_height = 0;

		if (pim_config->candidates_per_line == 0)			//注意除以0错误
			pim_config->candidates_per_line = 5;

		//设置候选的行、列数目
		candidate_cols = 1;
		candidate_rows = candidate_count = min(context->candidate_page_count, pim_config->candidates_per_line);
		candidate_line_width = ui_context->candidate_line_height = 0;

		//找出每一个候选的宽度以及高度，用于进行排列的计算
		for (i = 0; i < candidate_count; i++)
		{
			help_string[0] = 0;

			//获得字符串宽度
			if (pim_config->select_sytle == SELECTOR_DIGITAL)
				_stprintf_s(string, _SizeOf(string), TEXT("%1d%s"), i + 1, context->candidate_string[i]);
			else
				_stprintf_s(string, _SizeOf(string), TEXT("%c.%s"), 'a' + i, context->candidate_string[i]);

			if (context->english_state != ENGLISH_STATE_NONE && pim_config->use_english_input && pim_config->use_english_translate)
			{
				if (context->candidate_trans_string[i][0])
				{
					_tcscat_s(help_string, _SizeOf(help_string), context->candidate_trans_string[i]);
					_tcscat_s(help_string, _SizeOf(help_string), TEXT(" []"));
				}
			}
			else if((context->state == STATE_UINPUT || (context->state == STATE_ABC_SELECT && context->u_state)) &&
					(TCHAR *)context->candidate_array[i + context->candidate_index].spw.hint)
			{
				_tcscat_s(help_string, _SizeOf(help_string), (TCHAR *)context->candidate_array[i + context->candidate_index].spw.hint);
				_tcscat_s(help_string, _SizeOf(help_string), TEXT(" []"));
			}
			/*else if(context->url_state && context->candidate_array[i + context->candidate_index].url.hint)
			{
				_tcscat_s(help_string, _SizeOf(help_string), (TCHAR *)context->candidate_array[i + context->candidate_index].url.hint);
				_tcscat_s(help_string, _SizeOf(help_string), TEXT(" []"));
			}*/

			GetStringWidth(context, ui_context, string, (int)_tcslen(string), g, &width, &height, 0, 0);
			candidate_rect[i].top = width;			//暂时使用top记录真实宽度

			//列宽度
			width += CAND_NUMBER_GAP_WIDTH;
			if (width > candidate_col_width[0])
			{
				candidate_line_width  += width - candidate_col_width[0];
				candidate_col_width[0] = width;
			}

			if (width > max_candidate_width)
				max_candidate_width = width;

			//最高的列
			if (ui_context->candidate_line_height < height)
				ui_context->candidate_line_height = height;

			if (help_string[0])
			{
				//SIZE size;
				int cx, cy;

				//SelectObject(dc, ui_context->zi_font);
				//GetTextExtentPoint32(dc, help_string, (int)_tcslen(help_string), &size);
				GetStringWidth(context, ui_context, help_string, (int)_tcslen(help_string), g, &cx, &cy, ui_context->zi_font, 0);

				if (max_help_width < cx)
					max_help_width = cx;
			}
		}

		//整理候选的矩形
		for (i = 0; i < candidate_count; i++)
		{
			candidate_rect[i].left   = main_text_left_margin + candidate_offset;
			candidate_rect[i].right  = candidate_rect[i].left + candidate_rect[i].top;		//top中放置着宽度
			candidate_rect[i].top	 = ui_context->compose_frame_height + main_text_top_margin +
									   main_center_gap_height + CAND_GAP_HEIGHT * i + ui_context->candidate_line_height * i;
			candidate_rect[i].bottom = candidate_rect[i].top + ui_context->candidate_line_height;
		}

		if (pim_config->input_style == STYLE_ABC && context->state != STATE_ABC_SELECT && context->english_state != ENGLISH_STATE_INPUT)
			candidate_rows = ui_context->candidate_line_height = candidate_line_width = 0;
	}
	else
	{	//进行spw提示串的计算

		//获得字符串宽度
		GetStringWidth(context, ui_context, context->spw_hint_string, (int)_tcslen(context->spw_hint_string), g, &width, &height, 0, 0);

		candidate_line_width			  = width + 2 * CAND_GAP_WIDTH;
		ui_context->candidate_line_height = height;
		candidate_rect[0].left			  = main_text_left_margin;
		candidate_rect[0].right			  = candidate_rect[0].left + width;
		candidate_rect[0].top			  = ui_context->compose_frame_height + main_text_top_margin + main_center_gap_height;
		candidate_rect[0].bottom		  = candidate_rect[0].top + ui_context->candidate_line_height;
		candidate_rows					  = 1;
		candidate_cols					  = 0;
	}

	//设定主窗口的大小
	candidate_line_width += max_help_width;
	ui_context->main_window_size.cx =
		compose_width > candidate_line_width ? compose_width : candidate_line_width;

	if ((main_remain_number_margin >= 0) &&
		(ui_context->main_window_size.cx - main_remain_number_margin < compose_width + MIN_COMPOSE_NO_GAP - no_width))
		ui_context->main_window_size.cx += MIN_COMPOSE_NO_GAP;

	if (hint_right_margin >= 0)
	{
		if (ui_context->main_window_size.cx - hint_right_margin < compose_width + MIN_COMPOSE_HINT_GAP - hint_width)
			ui_context->main_window_size.cx += MIN_COMPOSE_HINT_GAP;

		if ((hint_right_margin > main_remain_number_margin) && (main_remain_number_margin >= 0))
			ui_context->main_window_size.cx += hint_right_margin - main_remain_number_margin;
	}

	if (max_candidate_width)
		max_candidate_width += main_text_left_margin;

	ui_context->main_window_size.cx += main_text_left_margin + main_text_right_margin;
	ui_context->main_window_size.cy = main_text_top_margin +											//文字上边界
									  main_text_bottom_margin +											//文字下边界
									  compose_height +													//写作框
									  (candidate_rows ? main_center_gap_height : 0) +					//写作、候选间隙
									  candidate_rows * ui_context->candidate_line_height +				//候选高度
									  (candidate_rows ? (candidate_rows - 1) * CAND_GAP_HEIGHT : 0);	//候选行间隙

	if (ui_context->main_window_size.cx < main_window_min_width)
		ui_context->main_window_size.cx = main_window_min_width;

	if (ui_context->main_window_size.cx < main_left_margin + main_right_margin)
		ui_context->main_window_size.cx = main_left_margin + main_right_margin;

	if (ui_context->main_window_size.cy < main_window_min_height)
		ui_context->main_window_size.cy = main_window_min_height;

	if (ui_context->main_window_size.cy < main_top_margin + main_bottom_margin)
		ui_context->main_window_size.cy = main_top_margin + main_bottom_margin;
}

/**	设定主窗口位置
 */
void SetMainWindowPosition(UICONTEXT *ui_context)
{
	RECT monitor_rect, window_rect;
	int	main_window_anchor_x, main_window_anchor_y;
	int use_vertical_bk = use_vertical_background(ui_context->context, ui_context, pim_config);

	main_window_anchor_x = use_vertical_bk ? pim_config->main_window_vert_anchor_x : pim_config->main_window_anchor_x;
	main_window_anchor_y = use_vertical_bk ? pim_config->main_window_vert_anchor_y : pim_config->main_window_anchor_y;

	monitor_rect = GetMonitorRectFromPoint(ui_context->caret_pos);

	if (pim_config->trace_caret)		//光标跟随
	{
		window_rect.left   = ui_context->caret_pos.x + CARET_X_OFFSET - main_window_anchor_x;
		window_rect.top    = ui_context->caret_pos.y + CARET_Y_OFFSET - main_window_anchor_y;
		window_rect.right  = ui_context->main_window_size.cx + window_rect.left;
		window_rect.bottom = ui_context->main_window_size.cy + window_rect.top;
	}
	else
	{
		window_rect.left   = pim_config->main_window_x - main_window_anchor_x;
		window_rect.top    = pim_config->main_window_y - main_window_anchor_y;
		window_rect.right  = ui_context->main_window_size.cx + window_rect.left;
		window_rect.bottom = ui_context->main_window_size.cy + window_rect.top;
	}

	MakeRectInRect(&window_rect, monitor_rect);

	//为了避免在窗口右侧出现的闪烁现象，窗口位置必须比原来的窗口位置小
	Log(LOG_ID, L"widow_rect.left:%d, window_pos.x:%d", window_rect.left, ui_context->main_window_pos.x);

	if (window_rect.left < ui_context->main_window_pos.x)
		ui_context->main_window_pos.x = window_rect.left;

	ui_context->main_window_pos.y = window_rect.top;

	SetWindowPos(ui_context->main_window,
				 0,
				 ui_context->main_window_pos.x,
				 ui_context->main_window_pos.y,
				 ui_context->main_window_size.cx,
				 ui_context->main_window_size.cy,
				 IME_WINDOW_FLAG);
}

/**	判断当前候选是否为ICW
 */
int IsICW(PIMCONTEXT *context, int index)
{
	if (context->candidate_array[index + context->candidate_index].type == CAND_TYPE_ICW)
		return 1;

	return 0;
}

void Draw_Text(Graphics &g, int x, int y, const TCHAR *str, Font *font, COLORREF c, int len, int is_transparent)
{
	int ret;
	//HDC dc = g.GetHDC();
	//Font font(dc, f);
	//g.ReleaseHDC(dc);
	PointF origin((REAL)x,(REAL)y);
	SolidBrush s_brush(Color(is_transparent ? 0x50 : 0xFE, GetRValue(c), GetGValue(c), GetBValue(c)));

	if(isSmooth == 1){
		g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
	}
	//g.SetSmoothingMode(SmoothingModeHighQuality);
	//g.SetTextContrast(0xffff);
	//g.SetCompositingQuality(CompositingQualityHighQuality);
	//g.SetCompositingMode(CompositingModeSourceOver);
	ret = g.DrawString(str, len, font, origin, &s_brush);

	//下面这段是赵国华提供的一套方案
	//HDC dc = g.GetHDC();
	//Font font(dc, f);

	//PointF origin((REAL)x,(REAL)y);
	//SolidBrush s_brush(Color(255, GetRValue(c), GetGValue(c), GetBValue(c)));

	//FontFamily fontFamily;
	//font.GetFamily(&fontFamily);
	//INT nStyle = font.GetStyle();
	//REAL size = font.GetSize();

	//g.ReleaseHDC(dc);
	//g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
	//GraphicsPath path;
	//path.AddString(str, len, &fontFamily, nStyle, size, origin, NULL);
	//g.SetSmoothingMode(SmoothingModeHighQuality);
	//g.FillPath(&s_brush, &path);
}

/**	绘制候选栏
 */
static void DrawCandidateFrame(PIMCONTEXT *context, UICONTEXT *ui_context, HDC dc, Graphics *g)
{
	COLORREF colorRef = 0, new_colorRef = 0;
	TCHAR string[4 + MAX_CANDIDATE_STRING_LENGTH];
	TCHAR help_string[MAX_TRANSLATE_STRING_LENGTH];
	TCHAR *p, *q;
	Font *aFont = NULL;
	int  candidate_count, candidates_per_line;
	int  x, y;
	int  i, length;
	int	 cx, cy;
	int	 syllable_index;
	int  transparent_text = 0;

	if (!context || !ui_context)
		return;

	if (!context->candidate_count && context->spw_hint_string[0])
	{
		x = candidate_rect[0].left;
		y = candidate_rect[0].top + ui_context->candidate_line_height;
		aFont = ui_context->zi_font;
		colorRef = (COLORREF)pim_config->main_candidate_color;
		p = context->spw_hint_string;
		length = (int)_tcslen(p);

		GetStringWidth(context, ui_context, p, length, *g, &cx, &cy, aFont, 0);
		Draw_Text(*g, x, y - cy, p, aFont, colorRef, length, 0);

		return;
	}

	if (pim_config->input_style == STYLE_ABC && context->state != STATE_ABC_SELECT &&
		context->english_state != ENGLISH_STATE_INPUT)
		return;

	candidates_per_line = pim_config->candidates_per_line;
	if (!candidates_per_line)
		candidates_per_line = 5;

	if (VIEW_MODE_EXPAND == context->candidates_view_mode)
		candidate_count = min(context->candidate_page_count, candidates_per_line * GetExpandCandidateLine());
	else
		candidate_count = min(context->candidate_page_count, candidates_per_line);

	if (pim_config->show_vertical_candidate)
		candidates_per_line = 1;

	//依次画出候选
	for (i = 0; i < candidate_count; i++)
	{
		int row = i / candidates_per_line;
		int col = i % candidates_per_line;

		x = candidate_rect[i].left;
		y = candidate_rect[i].top + ui_context->candidate_line_height;

		//画出候选序号
		aFont = ui_context->no_font;

		if (candidate_count > 1 && STATE_IINPUT == context->state && ENGLISH_STATE_NONE == context->english_state)
		{
			_stprintf_s(string, _SizeOf(string), TEXT("%c"), 'a' + i);
		}
		else if (/*context->candidate_array[i + context->candidate_index].type == CAND_TYPE_URL ||*/
				(pim_config->input_style == STYLE_CSTAR &&
				context->candidate_array[i + context->candidate_index].type == CAND_TYPE_SPW &&
				context->candidate_array[i + context->candidate_index].spw.type != SPW_STRING_NORMAL &&
				context->candidate_array[i + context->candidate_index].spw.type != SPW_STRING_BH &&
				context->candidate_array[i + context->candidate_index].spw.type != SPW_STIRNG_ENGLISH &&
				context->candidate_array[i + context->candidate_index].spw.type != SPW_STRING_EXEC))
		{
			string[0] = 0;
		}
		else if (VIEW_MODE_EXPAND == context->candidates_view_mode)
		{
			_stprintf_s(string, _SizeOf(string), TEXT("%1d%1d"), row + 1, col + 1);
		}
		else if (pim_config->input_style == STYLE_ABC && pim_config->select_sytle == SELECTOR_LETTER &&
				 context->english_state == ENGLISH_STATE_NONE)
			_stprintf_s(string, _SizeOf(string), TEXT("%c"), 'a' + i);
		else
			_stprintf_s(string, _SizeOf(string), TEXT("%1d"), i + 1);

		//设置当前选中的候选颜色
		if (i != context->candidate_selected_index)
		{
			colorRef = (COLORREF)pim_config->main_number_color;

			//如果为当前候选，则使用选中的颜色
			if (row + 1 == context->selected_digital)
			{		//当前行为选中行
				colorRef = (COLORREF)pim_config->main_selected_color;
			}
		}
		else
		{
			colorRef = (COLORREF)pim_config->main_selected_color;
		}

		//GetTextSize(dc, string, &width, &height);
		GetStringWidth(context, ui_context, string, (int)_tcslen(string), *g, &cx, &cy, aFont, 0);
		Draw_Text(*g, x, y - cy, string, aFont, colorRef, (int)_tcslen(string), 0);

		x += cx + CAND_NUMBER_GAP_WIDTH;

		//绘制候选
		//SelectObject(dc, ui_context->zi_font);
		aFont = ui_context->zi_font;

		//优先处理ICW颜色
		if (IsICW(context, i))
			colorRef = (COLORREF)pim_config->main_icw_color;
		else if (i != context->candidate_selected_index)
			colorRef = (COLORREF)pim_config->main_candidate_color;
		else
			colorRef = (COLORREF)pim_config->main_selected_color;

		new_colorRef = colorRef;

		_tcscpy_s(string, _SizeOf(string), context->candidate_string[i]);

		help_string[0] = 0;

		if (context->english_state != ENGLISH_STATE_NONE &&	pim_config->use_english_input && pim_config->use_english_translate)
		{
			if (context->candidate_trans_string[i][0])
			{
				_tcscat_s(help_string, _SizeOf(help_string), TEXT(" ["));
				_tcscat_s(help_string, _SizeOf(help_string), context->candidate_trans_string[i]);
				_tcscat_s(help_string, _SizeOf(help_string), TEXT("]"));
			}
		}
		//是u模式，但不是普通自定义短语时才显示这个help_string
		else if ((context->state == STATE_UINPUT || (context->state == STATE_ABC_SELECT && context->u_state)) &&
				 pim_config->u_mode_enabled && pim_config->use_u_hint 
				 && context->candidate_array[i + context->candidate_index].spw.hint
				 && context->candidate_array[i + context->candidate_index].spw.type != SPW_STRING_NORMAL)
		{
			_tcscat_s(help_string, _SizeOf(help_string), TEXT(" ["));
			_tcscat_s(help_string, _SizeOf(help_string), (TCHAR *)context->candidate_array[i + context->candidate_index].spw.hint);
			_tcscat_s(help_string, _SizeOf(help_string), TEXT("]"));
		}

		syllable_index = GetSyllableIndexByComposeCursor(context, context->compose_cursor_index);
		syllable_index = GetSyllableIndexInDefaultString(context, syllable_index);
		
		//下面的if具有如下效果：
		//输入menghaoke，第1个候选"孟毫克"，移动光标，但不要超过第一个音节(meng)，
		//如me|nghaoke，此时"孟"显示为已选中颜色。如果注释掉的话除非光标超过第一个
		//音节，否则"孟"显示为未选中颜色。其他音节是否加下面的if效果相同，都必须
		//超过该音节，该音节对应的字才会变成已选中的颜色
		//if (!syllable_index)
		//	syllable_index++;

		p = string;
		length = (int)_tcslen(p);
		while (p < string + length)
		{
			if (_IsNoneASCII(*p))			//汉字，寻找连续汉字
			{
				q = p + _HanZiLen;

				if (0 == i && context->syllable_mode && context->compose_cursor_index && 
					context->compose_cursor_index < context->compose_length)
				{
					while (q < string + length && (_IsNoneASCII(*q)) && 
						   q != string + syllable_index - (int)_tcslen(context->selected_compose_string))
						q += _HanZiLen;

					//对于默认选中的汉字，以半透明显示
					if (q == string + syllable_index - (int)_tcslen(context->selected_compose_string))
						transparent_text = 1;
					else
						transparent_text = 0;

					new_colorRef = colorRef;
				}
				else
				{
					while (q < string + length && (_IsNoneASCII(*q)))
						q += _HanZiLen;
				}
				aFont = ui_context->zi_font;
			}
			else					//英文，寻找连续英文
			{
				q = p + 1;
				while (q < string + length && (!_IsNoneASCII(*q)))
					q++;
				aFont = ui_context->ascii_font;
			}

			//获得串的size
			GetStringWidth(context, ui_context, p, (int)(q - p), *g, &cx, &cy, aFont, 0);
			Draw_Text(*g, x, y - cy, p, aFont, new_colorRef, (int)(q - p), transparent_text);
			x += cx;
			p = q;
		}

		if (help_string[0] && max_candidate_width)
		{
			aFont = ui_context->zi_font;
			GetStringWidth(context, ui_context, help_string, (int)_tcslen(help_string), *g, &cx, &cy, aFont, 0);
			Draw_Text(*g, max_candidate_width,  y - cy, help_string, aFont, colorRef, (int)_tcslen(help_string), 0);
		}
	}
}

int myCeil(int n, int k){
	int i = n / k;
	int m = n % k;
	if(m > 0)
		return i+1;
	else
		return i;
}

/*	画写作内容。
 *	需要注意：
 *	1. 中文字体、颜色
 *	2. 西文字体、颜色
 *	3. 光标位置、颜色
 *	如：我men2de
 */
static void DrawCompositionFrame(PIMCONTEXT *context, UICONTEXT *ui_context, HDC dc, Graphics *g)
{
	COLORREF colorRef = 0;
	Font *aFont = NULL;
	static TCHAR *tone_string;
	const TCHAR *p, *q;
	TCHAR *no_string;
	int x, y, width, height, abc_width, abc_height, cx, cy;
	int no_width = 0, no_height = 0;
	int main_remain_number_margin, main_text_left_margin;
	int hint_right_margin = 0;
	TCHAR hint_string[MAX_CAND_HINT_SIZE] = {0};
	int	caret_x, count = 0;

	TESTCONTEXTS;
	p = q = context->state == STATE_IEDIT ? (TCHAR*)context->iedit_hz : context->compose_string;
	//中文字体作为Compose窗口的计算字体
	GetStringWidth(context, ui_context, p, (int)_tcslen(p), *g, &width, &height, ui_context->zi_font, 0);
	int use_vertical_bk = use_vertical_background(context, ui_context, pim_config);
	x = use_vertical_bk ? pim_config->main_vert_text_left_margin : pim_config->main_text_left_margin;
	y = use_vertical_bk ? pim_config->main_vert_text_top_margin : pim_config->main_text_top_margin;
	main_remain_number_margin = use_vertical_bk ? pim_config->main_vert_remain_number_margin : pim_config->main_remain_number_margin;
	main_text_left_margin	  = use_vertical_bk ? pim_config->main_vert_text_left_margin : pim_config->main_text_left_margin;
	hint_right_margin		  = use_vertical_bk ? pim_config->hint_vert_right_margin : pim_config->hint_right_margin;

	if (context->state == STATE_ABC_SELECT)
	{
		//获取候选序号的宽度
		if (pim_config->input_style == STYLE_ABC && pim_config->select_sytle == SELECTOR_LETTER)
			no_string = TEXT("a");
		else
			no_string = TEXT("1");
		GetStringWidth(context, ui_context, no_string, (int)_tcslen(no_string), *g, &abc_width, &abc_height, 0, 0);
		x += abc_width;
	}

	caret_x = x;
	if (context->state == STATE_ABC_SELECT)
	{
		TCHAR *selected_string = GetSelectedComposeString(context);
		TCHAR *cand_string = GetCurrentCandidateString(context);
		TCHAR *remain_string = GetReaminComposeString(context);

		colorRef = (COLORREF)pim_config->main_string_color;
		if (context->u_state)
			aFont = ui_context->ascii_font;
		else
			aFont = ui_context->zi_font;
		GetStringWidth(context, ui_context, selected_string, (int)_tcslen(selected_string), *g, &cx, &cy, aFont, 0);
		Draw_Text(*g,x, y + height - cy, selected_string, aFont, colorRef, (int)_tcslen(selected_string), 0);
		x += cx;
		colorRef = (COLORREF)pim_config->main_selected_color;
		GetStringWidth(context, ui_context, cand_string, (int)_tcslen(cand_string), *g, &cx, &cy, aFont, 0);
		Draw_Text(*g, x, y + height - cy, cand_string, aFont, colorRef, (int)_tcslen(cand_string), 0);
		x += cx;
		colorRef = (COLORREF)pim_config->main_string_color;
		for (p = remain_string; *p; p = q)
		{
			if (context->syllable_count && *p >= '1' && *p <= '4')
			{
				//拼音音调
				q = p + 1;
				GetStringWidth(context, ui_context, p, 1, *g, &cx, &cy, ui_context->tone_font, 0);
				Draw_Text(*g, x, y + height * 2 / 3 - cy, p, ui_context->tone_font, colorRef, 1, 0);
				x += cx;
				continue;
			}
			for (q = p + 1; *q && (*q < '1' || *q > '4'); q++)
				;
			GetStringWidth(context, ui_context, p, (int)(q - p), *g, &cx, &cy, ui_context->ascii_font, 0);
			Draw_Text(*g,x, y + height - cy, p, ui_context->ascii_font, colorRef, (int)(q - p), 0);
			x += cx;
		}
	}
	else
	{
		int is_tone;

		colorRef = (COLORREF)pim_config->main_string_color;
		for (; *p; p = q)
		{
			is_tone = 0;
			if (_IsNoneASCII(*p))											//中文
				aFont = ui_context->zi_font;
			else if (context->english_state == ENGLISH_STATE_NONE && context->syllable_count && *p >= '1' && *p <= '4')		//如果是数字，并且处于拼音状态，则以小字体的方式进行绘制
			{
				is_tone = 1;
				aFont = ui_context->tone_font;
			}
			else															//英文
				aFont = ui_context->ascii_font;
			q++;
			GetStringWidth(context, ui_context, p, 1, *g, &cx, &cy, aFont, 0);
			if (is_tone)
				Draw_Text(*g, x, y + height * 2 / 3 - cy, p, aFont, colorRef, 1, 0);
			else
				Draw_Text(*g, x, y + height - cy, p, aFont, colorRef, 1, 0);
			x += cx;
			count++;
			if ((context->state != STATE_IEDIT && count == context->compose_cursor_index) || 
				(context->state == STATE_IEDIT && count == context->iedit_syllable_index))
				caret_x = x;
		}

		//绘制双拼提示串
		if (pim_config->pinyin_mode == PINYIN_SHUANGPIN && 	pim_config->show_sp_hint &&
			context->state == STATE_EDIT && context->sp_hint_string[0])
		{
			TCHAR str[0x100] = TEXT("[");
			_tcscat_s(str, _SizeOf(str), context->sp_hint_string);
			_tcscat_s(str, TEXT("]"));
			x += COMPOSE_SP_HINT_GAP;
			aFont = ui_context->ascii_font;
			GetStringWidth(context, ui_context, str, (int)_tcslen(str), *g, &cx, &cy, aFont, 1);
			Draw_Text(*g, x, y + height - cy, str, aFont, colorRef, (int)_tcslen(str), 0);
			x += cx;
		}
	}

	if (context->candidate_count && (main_remain_number_margin >= 0) && //有候选的情况下，才进行绘制
		!(pim_config->input_style == STYLE_ABC && context->state != STATE_ABC_SELECT && context->english_state == ENGLISH_STATE_NONE))
	{
		int countperpage/*每页显示多少*/;
		int currentpage;//当前页
		int pagecount;//总页数
		int  offset_x;
		TCHAR no_string[0x10];

		if (context->candidate_count > 0)
		{
			//cand_remains = context->candidate_count - countperpage;
			if (VIEW_MODE_EXPAND == context->candidates_view_mode)
				countperpage = pim_config->candidates_per_line * GetExpandCandidateLine();
			else
				countperpage = pim_config->candidates_per_line;
			if(context->candidate_index % countperpage){
				currentpage = myCeil(context->candidate_index, countperpage) + 1;
				pagecount = currentpage + myCeil(context->candidate_count - context->candidate_index, countperpage) - 1;
			}else{
				currentpage = myCeil(context->candidate_index + 1, countperpage);
				pagecount = myCeil(context->candidate_count, countperpage);
			}
			_stprintf_s(no_string, _SizeOf(no_string), TEXT("%d/%d"), currentpage, pagecount);

			aFont    = ui_context->tone_font;
			colorRef = (COLORREF)pim_config->main_remains_number_color;
			GetStringWidth(context, ui_context, no_string, (int)_tcslen(no_string), *g, &no_width, &no_height, aFont, 1);
			offset_x = ui_context->main_window_size.cx - main_remain_number_margin - no_width;
			if (offset_x < x)		//与composec串重叠
				offset_x = x + 4;
			Draw_Text(*g, offset_x, y + height - no_height, no_string, aFont, colorRef, (int)_tcslen(no_string), 0);
		}
	}

	//获取提示串
	if(pim_config->show_hint_msg)
		GetHintString(context, hint_string);
	else
		hint_string[0]=0;

	if (hint_string[0] && (hint_right_margin >= 0))
	{
		int  hint_width, hint_height;
		int  offset_x;

		colorRef = (COLORREF)pim_config->main_string_color;

		GetStringWidth(context, ui_context, hint_string, (int)_tcslen(hint_string), *g, &hint_width, &hint_height, ui_context->guide_font, 0);

		if (hint_right_margin)
			offset_x = ui_context->main_window_size.cx - hint_width - hint_right_margin;
		else if (main_remain_number_margin > 0)
			offset_x = ui_context->main_window_size.cx - main_remain_number_margin - no_width - hint_width - MIN_HINT_NO_GAP;
		else
			offset_x = ui_context->main_window_size.cx - no_width - hint_width - MIN_HINT_NO_GAP;
		if (offset_x < x)		//与composec串重叠
			offset_x = x + 10;
		Draw_Text(*g, offset_x, y + height - hint_height, hint_string, ui_context->guide_font, colorRef, (int)_tcslen(hint_string), 0);
	}

	//光标
	if (show_cursor && context->state != STATE_ABC_SELECT)
	{
		Pen pen1(Color(254, GetRValue((COLORREF)pim_config->main_caret_color),
							GetGValue((COLORREF)pim_config->main_caret_color),
							GetBValue((COLORREF)pim_config->main_caret_color)));

		g->DrawLine(&pen1, COMPOSE_CARET_GAP_WIDTH + caret_x, y, COMPOSE_CARET_GAP_WIDTH + caret_x, y + height);
	}
}

static void DrawMainAssistLine(PIMCONTEXT *context, UICONTEXT *ui_context, Graphics *g)
{
	if (!global_draw_assist_line)
		return;

	int main_left_margin, main_right_margin, main_top_margin, main_bottom_margin;
	int main_window_anchor_x, main_window_anchor_y;
	int use_vertical_bk = use_vertical_background(context, ui_context, pim_config);

	main_left_margin	 = use_vertical_bk ? pim_config->main_vert_left_margin	   : pim_config->main_left_margin;
	main_right_margin	 = use_vertical_bk ? pim_config->main_vert_right_margin	   : pim_config->main_right_margin;
	main_top_margin		 = use_vertical_bk ? pim_config->main_vert_top_margin	   : pim_config->main_top_margin;
	main_bottom_margin	 = use_vertical_bk ? pim_config->main_vert_bottom_margin   : pim_config->main_bottom_margin;
	main_window_anchor_x = use_vertical_bk ? pim_config->main_window_vert_anchor_x : pim_config->main_window_anchor_x;
	main_window_anchor_y = use_vertical_bk ? pim_config->main_window_vert_anchor_y : pim_config->main_window_anchor_y;

	Pen pen(Color(255, 0, 0));		//红色
	g->DrawLine(&pen, main_left_margin, 0, main_left_margin, ui_context->main_window_size.cy - 1);
	g->DrawLine(&pen, ui_context->main_window_size.cx - main_right_margin - 1, 0, ui_context->main_window_size.cx - main_right_margin - 1, ui_context->main_window_size.cy - 1);
	g->DrawLine(&pen, 0, main_top_margin, ui_context->main_window_size.cx - 1, main_top_margin);
	g->DrawLine(&pen, 0, ui_context->main_window_size.cy - main_bottom_margin - 1, ui_context->main_window_size.cx - 1, ui_context->main_window_size.cy - main_bottom_margin - 1);
	g->DrawEllipse(&pen, main_window_anchor_x, main_window_anchor_y, 4, 4);
}

static void DrawStatusAssistLine(UICONTEXT *ui_context, Graphics *g)
{
	if (!global_draw_assist_line)
		return;

	Pen pen(Color(255, 0, 0));		//红色
	g->DrawLine(&pen, pim_config->status_left_margin, 0, pim_config->status_left_margin, status_window_height - 1);
	g->DrawLine(&pen, status_window_width - pim_config->status_right_margin, 0, status_window_width - pim_config->status_right_margin, status_window_height - 1);
	g->DrawLine(&pen, 0, pim_config->status_window_top_margin, status_window_width -1, pim_config->status_window_top_margin);
}

int NeedCenterLine(PIMCONTEXT *context)
{
	return context->state == STATE_ABC_SELECT ||
			(context->english_state == ENGLISH_STATE_INPUT && context->candidate_count) ||
			(pim_config->input_style != STYLE_ABC && context->candidate_count &&
			 context->show_candidates && context->show_composition) ||
			(pim_config->input_style != STYLE_ABC && !context->candidate_count && context->spw_hint_string[0]);
}

//sunmd：判断是否透明度过大，字体需要特殊渲染
int soTransparent(int alpha)
{
	return (alpha >= 0) && (alpha < 255);
}

void DrawMainBackground(PIMCONTEXT *context, UICONTEXT *ui_context, Graphics *g, int alpha)
{
	Color color;
	Bitmap *image_main_bk, *image_main_line;

	if (!context || !ui_context || !g)
		return;

	//没有背景图像，则使用默认颜色
	if (!ui_context->image_main_bk)
	{
		SolidBrush brush(Color(alpha, 160, 160, 160));
		g->FillRectangle(&brush, 0, 0, ui_context->main_window_size.cx, ui_context->main_window_size.cy);

		Pen pen(Color(alpha, 0, 0, 0));
		g->DrawRectangle(&pen, 0, 0, ui_context->main_window_size.cx - 1, ui_context->main_window_size.cy - 1);

		if (NeedCenterLine(context))
		{
			pen.SetColor(Color(alpha,160, 160, 160));
			g->DrawLine(&pen,
				pim_config->main_line_left_margin,
				ui_context->compose_frame_height + pim_config->main_text_top_margin,
				ui_context->main_window_size.cx - pim_config->main_line_right_margin,
				ui_context->compose_frame_height + pim_config->main_text_top_margin);
		}

		DrawMainAssistLine(context, ui_context, g);
		return;
	}

	//使用背景图像
	int left_margin, right_margin, top_margin, bottom_margin;
	int center_mode, vertical_mode, horizontal_mode;
	int main_line_mode, main_line_left_margin, main_line_right_margin, main_text_top_margin,main_text_left_margin,main_hz_top_margin;
	int main_center_gap_height, tag_width, tag_height, src_width, src_height;;
	int &win_width		= (int&)ui_context->main_window_size.cx;
	int &win_height		= (int&)ui_context->main_window_size.cy;
	int use_vertical_bk = use_vertical_background(context, ui_context, pim_config); //显示竖排背景

	image_main_bk			= use_vertical_bk ? ui_context->image_main_vert_bk : ui_context->image_main_bk;
	image_main_line			= use_vertical_bk ? ui_context->image_main_vert_line : ui_context->image_main_line;
	left_margin				= use_vertical_bk ? pim_config->main_vert_left_margin : pim_config->main_left_margin;
	right_margin			= use_vertical_bk ? pim_config->main_vert_right_margin : pim_config->main_right_margin;
	top_margin				= use_vertical_bk ? pim_config->main_vert_top_margin : pim_config->main_top_margin;
	bottom_margin			= use_vertical_bk ? pim_config->main_vert_bottom_margin : pim_config->main_bottom_margin;
	center_mode				= use_vertical_bk ? pim_config->main_vert_center_mode : pim_config->main_center_mode;
	vertical_mode			= use_vertical_bk ? pim_config->main_vert_vertical_mode : pim_config->main_vertical_mode;
	horizontal_mode			= use_vertical_bk ? pim_config->main_vert_horizontal_mode : pim_config->main_horizontal_mode;
	main_line_mode			= use_vertical_bk ? pim_config->main_vert_line_mode : pim_config->main_line_mode;
	main_line_left_margin	= use_vertical_bk ? pim_config->main_vert_line_left_margin : pim_config->main_line_left_margin;
	main_line_right_margin	= use_vertical_bk ? pim_config->main_vert_line_right_margin : pim_config->main_line_right_margin;
	main_text_top_margin	= use_vertical_bk ? pim_config->main_vert_text_top_margin : pim_config->main_text_top_margin;
	main_text_left_margin	= use_vertical_bk ? pim_config->main_vert_text_left_margin : pim_config->main_text_left_margin;
	main_center_gap_height	= use_vertical_bk ? pim_config->main_vert_center_gap_height : pim_config->main_center_gap_height;
	main_hz_top_margin      = ui_context->compose_frame_height + main_text_top_margin + main_center_gap_height;

	if (top_margin + bottom_margin > win_height)
	{
		if (top_margin >= bottom_margin)
			top_margin = win_height - bottom_margin;
		else
			bottom_margin = win_height - top_margin;

		if (top_margin < 0)
			top_margin = 0;

		if (bottom_margin < 0)
			bottom_margin = 0;
	}

	if (left_margin + right_margin > win_width)
	{
		if (left_margin >= right_margin)
			left_margin = win_width - right_margin;
		else
			right_margin = win_width - left_margin;

		if (left_margin < 0)
			left_margin = 0;

		if (right_margin < 0)
			right_margin = 0;
	}

	//如果图片透明度太高，需要处理，否则字会有毛边
	Color c1,c2;
	image_main_bk->GetPixel(main_text_left_margin, main_text_top_margin, &c1);
	image_main_bk->GetPixel(main_text_left_margin, main_hz_top_margin, &c2);
	isSmooth = 0;
	if (soTransparent(c1.GetAlpha()) || soTransparent(c2.GetAlpha()))
		isSmooth = 1;
	
	//清理背景图像，全部设置为透明色
	SolidBrush brush(Color(alpha, 255, 254, 255));
	g->FillRectangle(&brush, 0, 0, ui_context->main_window_size.cx, ui_context->main_window_size.cy);

	ImageAttributes	image_attribute;
	image_attribute.SetColorKey(Color(255, 254, 255), Color(255, 254, 255), ColorAdjustTypeBitmap);

	//绘制左上角
	g->DrawImage(image_main_bk,
		Rect(0, 0, left_margin, top_margin),
		0, 0, left_margin, top_margin,
		UnitPixel);

	//绘制右上角
	g->DrawImage(image_main_bk,
		Rect(ui_context->main_window_size.cx - right_margin, 0, right_margin, top_margin),
		image_main_bk->GetWidth() - right_margin, 0,
		right_margin, top_margin,
		UnitPixel);

	//绘制左下角
	g->DrawImage(image_main_bk,
		Rect(0, ui_context->main_window_size.cy - bottom_margin, left_margin, bottom_margin),
		0, image_main_bk->GetHeight() - bottom_margin,
		left_margin, bottom_margin,
		UnitPixel);

	//绘制右下角
	g->DrawImage(image_main_bk,
		Rect(ui_context->main_window_size.cx - right_margin, ui_context->main_window_size.cy - bottom_margin, right_margin, bottom_margin),
		image_main_bk->GetWidth() - right_margin, image_main_bk->GetHeight() - bottom_margin,
		right_margin, bottom_margin,
		UnitPixel);

	//设置为平铺模式
	image_attribute.SetWrapMode(WrapModeTile);

	//绘制左边、右边
	tag_height = ui_context->main_window_size.cy - top_margin - bottom_margin;
	src_height = image_main_bk->GetHeight() - top_margin - bottom_margin;

	//平铺
	if (vertical_mode == DRAW_MODE_TILED)
	{
		Bitmap *left_line  = image_main_bk->Clone(0, top_margin, left_margin, src_height, PixelFormatDontCare);
		Bitmap *right_line = image_main_bk->Clone(image_main_bk->GetWidth() - right_margin, top_margin, right_margin, src_height, PixelFormatDontCare);

		g->DrawImage(left_line,
			Rect(0, top_margin, left_margin, tag_height),
			0, 0, left_margin, tag_height,
			UnitPixel,
			&image_attribute);

		g->DrawImage(right_line,
			Rect(ui_context->main_window_size.cx - right_margin, top_margin, right_margin, tag_height),
			0, 0, right_margin, tag_height,
			UnitPixel,
			&image_attribute);

		delete left_line;
		delete right_line;
	}
	else
	{	//拉伸方式
		g->DrawImage(image_main_bk,
			Rect(0, top_margin, left_margin, tag_height),
			0, top_margin, left_margin, src_height,
			UnitPixel);

		g->DrawImage(image_main_bk,
			Rect(ui_context->main_window_size.cx - right_margin, top_margin, right_margin, tag_height),
			image_main_bk->GetWidth() - right_margin, top_margin, right_margin, src_height,
			UnitPixel);
	}

	//绘制上边、下面
	tag_width = ui_context->main_window_size.cx - left_margin - right_margin;
	src_width = image_main_bk->GetWidth() - left_margin - right_margin;

	if (horizontal_mode == DRAW_MODE_TILED)
	{
		Bitmap *top_line    = image_main_bk->Clone(left_margin, 0, src_width, top_margin, PixelFormatDontCare);
		Bitmap *bottom_line = image_main_bk->Clone(left_margin, image_main_bk->GetHeight() - bottom_margin, src_width, bottom_margin, PixelFormatDontCare);

		g->DrawImage(top_line,
			Rect(left_margin, 0, tag_width, top_margin),
			0, 0, tag_width, top_margin,
			UnitPixel,
			&image_attribute);

		g->DrawImage(bottom_line,
			Rect(left_margin, ui_context->main_window_size.cy - bottom_margin, tag_width, bottom_margin),
			0, 0,
			tag_width, bottom_margin,
			UnitPixel,
			&image_attribute);

		delete top_line;
		delete bottom_line;
	}
	else				//拉伸方式
	{
		g->DrawImage(image_main_bk,
			Rect(left_margin, 0, tag_width, top_margin),
			left_margin, 0, src_width, top_margin,
			UnitPixel);

		g->DrawImage(image_main_bk,
			Rect(left_margin, ui_context->main_window_size.cy - bottom_margin, tag_width, bottom_margin),
			left_margin, image_main_bk->GetHeight() - bottom_margin, src_width, bottom_margin,
			UnitPixel);
	}

	//绘制中心图像
	tag_width  = ui_context->main_window_size.cx - left_margin - right_margin;
	tag_height = ui_context->main_window_size.cy - top_margin - bottom_margin;
	src_width  = image_main_bk->GetWidth() - left_margin - right_margin;
	src_height = image_main_bk->GetHeight() - top_margin - bottom_margin;

	if (center_mode == DRAW_MODE_TILED)		//平铺模式
	{
		Bitmap *center_image = image_main_bk->Clone(left_margin, top_margin, src_width, src_height, PixelFormatDontCare);

		g->DrawImage(center_image,
			Rect(left_margin, top_margin, tag_width, tag_height),
			0, 0, tag_width, tag_height,
			UnitPixel,
			&image_attribute);

		delete center_image;
	}
	else									//拉伸方式
	{
		g->DrawImage(image_main_bk,
			Rect(left_margin, top_margin, tag_width, tag_height),
			left_margin, top_margin, src_width, src_height,
			UnitPixel);
	}

	//画中心线，只有当候选与写作同时显示的时候，才需要画中心分割线
	if (image_main_line && NeedCenterLine(context))
	{
		if (main_line_mode == DRAW_MODE_STRETCH) //拉伸模式
		{
			g->DrawImage(image_main_line,
				main_line_left_margin,
				ui_context->compose_frame_height + main_text_top_margin + main_center_gap_height / 2 - image_main_line->GetHeight() / 2,
				ui_context->main_window_size.cx - main_line_right_margin - main_line_left_margin,
				image_main_line->GetHeight());
		}
		else //平铺模式
		{
			ImageAttributes	image_attribute;
			image_attribute.SetWrapMode(WrapModeTile);

			g->DrawImage(image_main_line,
				Rect(main_line_left_margin,
					 ui_context->compose_frame_height + main_text_top_margin + main_center_gap_height / 2 - image_main_line->GetHeight() / 2,
					 ui_context->main_window_size.cx - main_line_right_margin - main_line_left_margin,
					 image_main_line->GetHeight()),
				0, 0,
				ui_context->main_window_size.cx - main_line_right_margin - main_line_left_margin,
				image_main_line->GetHeight(),
				UnitPixel,
				&image_attribute);
		}
	}

	DrawMainAssistLine(context, ui_context, g);
	g->Flush();
	return;
}

HBITMAP GetHBitMapFromRect(int width, int height)
{
	BYTE * pBits ;
	BITMAPINFOHEADER bmih;

	ZeroMemory(&bmih, sizeof(BITMAPINFOHEADER));

	bmih.biSize			 = sizeof(BITMAPINFOHEADER);
	bmih.biWidth		 = width;
	bmih.biHeight		 = height;
	bmih.biPlanes		 = 1;
	bmih.biBitCount		 = 32;		//这里一定要是32
	bmih.biCompression	 = BI_RGB;
	bmih.biSizeImage	 = 0;
	bmih.biXPelsPerMeter = 0;
	bmih.biYPelsPerMeter = 0;
	bmih.biClrUsed		 = 0;
	bmih.biClrImportant  = 0;

	return CreateDIBSection (NULL, (BITMAPINFO *) &bmih, 0, (VOID**)&pBits, NULL, 0);
}

//在DC上进行compose与candidate的输出
void PaintMainWindow(PIMCONTEXT *context, UICONTEXT *ui_context, HDC dc)
{
	//todo:本函数调用次数有些多。
	Log(LOG_ID, L"绘制界面");

	if (!context || (!context->show_composition && !context->show_candidates) || !ui_context)
		return;

	HWND hWnd = WindowFromDC(dc);
	RECT rect;
	GetWindowRect(hWnd,&rect);

	POINT pt  = {0, 0};
	POINT pos = {rect.left, rect.top};

	HBITMAP	bitmap = 0;

	//当size超过bmp的大小的时候，ULW函数不报错，但没有任何动作。
	SIZE size = {rect.right - rect.left, rect.bottom - rect.top};

	//Alpha通道窗口必须每次都要设置EX_STYLE，否则出错！
	SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd,GWL_EXSTYLE) | WS_EX_LAYERED);

	HDC m_dc = CreateCompatibleDC(dc);
	bitmap = GetHBitMapFromRect(rect.right - rect.left, rect.bottom - rect.top);//CreateCompatibleBitmap(dc, rect.right-rect.left,rect.bottom-rect.top);//ui_context->main_window_size.cx, ui_context->main_window_size.cy);
	SelectObject(m_dc, bitmap);

	Graphics g(m_dc);
	//g.SetTextRenderingHint(TextRenderingHintSingleBitPerPixelGridFit);
	DrawMainBackground(context, ui_context, &g, 0);

	//画内容
	DrawCompositionFrame(context, ui_context, m_dc, &g);
	DrawCandidateFrame(context, ui_context, m_dc, &g);

	BLENDFUNCTION blend_function = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

	UpdateLayeredWindow(hWnd, 0, &pos, &size, m_dc, &pt, 0, &blend_function, ULW_ALPHA);

	DeleteObject(m_dc);
	DeleteObject(bitmap);
}

//配置界面/主题制作:在DC上进行compose与candidate的输出
void PaintMainCanvas(PIMCONTEXT *context, UICONTEXT *ui_context, HDC dc)
{
	HDC		memory_dc;
	//HPEN	pen = 0, old_pen = 0;
	//HBRUSH	brush = 0, old_brush = 0;
	HBITMAP	bitmap;
	int st = clock();

	//todo:本函数调用次数有些多。
	Log(LOG_ID, L"绘制界面");
	if (!context || (!context->show_composition && !context->show_candidates) || !ui_context)
		return;
	memory_dc = CreateCompatibleDC(dc);
	bitmap = CreateCompatibleBitmap(dc, ui_context->main_window_size.cx, ui_context->main_window_size.cy);
	SelectObject(memory_dc, bitmap);
	//设置透明模式，用于显示背景图像
	SetBkMode(memory_dc, TRANSPARENT);
	//初始化图形对象
	Graphics g(memory_dc);
	DrawMainBackground(context, ui_context, &g, 254);

	//画内容
	DrawCompositionFrame(context, ui_context, memory_dc, &g);
	DrawCandidateFrame(context, ui_context, memory_dc, &g);
	BitBlt(dc, 0, 0, ui_context->main_window_size.cx, ui_context->main_window_size.cy, memory_dc, 0, 0, SRCCOPY);
	DeleteObject(memory_dc);
	DeleteObject(bitmap);
	//if (pen)
	//	DeleteObject(pen);
	//if (brush)
	//	DeleteObject(brush);
	//if (old_pen)
	//	SelectObject(dc, old_pen);
	//if (old_brush)
	//	SelectObject(dc, old_brush);
	Log(LOG_ID, L"Cost:%d", clock() - st);
}

/**	获得在主窗口上的鼠标位置信息
 */
int GetMainCursorZone(PIMCONTEXT *context, UICONTEXT *ui_context, int *index)
{
	int   i;
	POINT cursor_pos;				//光标在屏幕中的位置

	if (!context)
		return ZONE_NONE;

	GetCursorPos(&cursor_pos);
	ScreenToClient(ui_context->main_window, &cursor_pos);

	*index = 0;
	for (i = 0; i < context->candidate_page_count; i++)
	{
		if (PtInRect(&candidate_rect[i], cursor_pos))
		{
			*index = i;
			return ZONE_CANDIDATE;
		}
	}

	int main_text_top_margin;

	if (use_vertical_background(context, ui_context, pim_config))
		main_text_top_margin = pim_config->main_vert_text_top_margin;
	else
		main_text_top_margin = pim_config->main_text_top_margin;

	//在写作框内，则返回左右区别
	if (cursor_pos.y < ui_context->compose_frame_height + main_text_top_margin)
	{
		if (cursor_pos.x < ui_context->main_window_size.cx / 2)
			return ZONE_COMPOSE_LEFT;

		return ZONE_COMPOSE_RIGHT;
	}

	return ZONE_NONE;
}

void RefreshMainWindowPosition(UICONTEXT *ui_context)
{
	RECT rect;

	GetWindowRect(ui_context->main_window, &rect);

	if (pim_config->trace_caret)
	{
		ui_context->caret_pos.x += (rect.left - ui_context->main_window_pos.x);
		ui_context->caret_pos.y += (rect.top - ui_context->main_window_pos.y);

		ui_context->main_window_pos.x = rect.left;
		ui_context->main_window_pos.y = rect.top;
	}
	else
	{
		pim_config->main_window_x = rect.left;
		pim_config->main_window_y = rect.top;
		Log(LOG_ID, L"记录主窗口的位置");
		SaveConfigInternal(pim_config);
	}
}
//查找中文字体名称对应的英文字体名称
//count:字体名称长度，用于_tcscpy
//写这个方法主要是因为在英文版win8中，不识别类似“宋体”的字体，而识别“simsun”
void transformFont(TCHAR *oldFontName, TCHAR *newFontName, int count, TCHAR *defaultFontName)
{
	static TCHAR *mapFont[] =
	{
		TEXT("宋体"),       TEXT("SimSun"),
		TEXT("微软雅黑"),   TEXT("Microsoft YaHei"),
		TEXT("新宋体"),	    TEXT("NSimSum"),
		TEXT("仿宋"),       TEXT("FangSong"),
		TEXT("黑体"),       TEXT("SimHei"),
		TEXT("楷书"),       TEXT("KaiTi"),
		TEXT("隶书"),       TEXT("LiSu"),
		TEXT("幼圆"),       TEXT("YouYuan"),
		TEXT("方正舒体"),   TEXT("FZShuTi"),
		TEXT("方正姚体"),   TEXT("FZYaoTi"),
		TEXT("华文彩云"),   TEXT("STCaiyun"),
		TEXT("华文仿宋"),   TEXT("STFangsong"),
		TEXT("华文行楷"),   TEXT("STXingkai"),
		TEXT("华文琥珀"),   TEXT("STHupo"),
		TEXT("华文楷体"),   TEXT("STKaiti"),
		TEXT("华文隶书"),   TEXT("STLiti"),
		TEXT("华文宋体"),   TEXT("STSong"),
		TEXT("华文细黑"),   TEXT("STXihei"),
		TEXT("华文新魏"),   TEXT("STXinwei"),
		TEXT("华文中宋"),   TEXT("STZhongsong"),
		TEXT("华文中宋"),   TEXT("STZhongsong"),
	};
	FontFamily ff(oldFontName);
	if(ff.IsAvailable())
	{
		_tcscpy_s(newFontName, count, oldFontName);
		return;
	}
	for ( int i = 0; i < sizeof(mapFont) / sizeof(int); i = i+2)
	{
		if(!_tcscmp(mapFont[i], oldFontName))
		{
			FontFamily ff1(mapFont[i+1]);
			if(ff1.IsAvailable())
			{
				_tcscpy_s(newFontName, count, mapFont[i+1]);
				return;
			}
		}
	}
	_tcscpy_s(newFontName, count, defaultFontName);
}

//检查字体font是否正常，如果不正常，使用默认fontfamily重新创建
void CheckCreateFont( Font **font, FontFamily *ff, FontFamily *ffDefault, REAL height, INT style, Unit unit )
{
	*font = new Font(ff, height, style, unit);
	if((*font)->GetLastStatus()!=Ok){
		delete *font;
		*font = new Font(ffDefault, height, style, unit);
	}
}

/**	创建字体
 */
static void PIM_CreateFonts(UICONTEXT *ui_context)
{
	TCHAR cnFontName[MAX_FILE_NAME_LENGTH], enFontName[MAX_FILE_NAME_LENGTH];
	transformFont(pim_config->chinese_font_name, cnFontName, _SizeOf(cnFontName), TEXT("SimSun"));
	transformFont(pim_config->english_font_name, enFontName, _SizeOf(enFontName), TEXT("Arial"));
	FontFamily ffCn(cnFontName);
	FontFamily ffEn(enFontName);
	FontFamily ffCnDefault(TEXT("SimSun"));
	FontFamily ffEnDefault(TEXT("Arial"));
	REAL fontHeight = REAL(pim_config->font_height);

	CheckCreateFont(&ui_context->zi_font, &ffCn, &ffCnDefault, fontHeight, FontStyleRegular, UnitPixel);
	CheckCreateFont(&ui_context->no_font, &ffCn, &ffCnDefault, fontHeight * 85 / 100, FontStyleRegular, UnitPixel);
	CheckCreateFont(&ui_context->guide_font, &ffCn, &ffCnDefault, fontHeight * 4 / 5, FontStyleRegular, UnitPixel);

	CheckCreateFont(&ui_context->ascii_font, &ffEn, &ffEnDefault, fontHeight, FontStyleBold, UnitPixel);
	CheckCreateFont(&ui_context->tone_font, &ffEn, &ffEnDefault, fontHeight * 2 / 3, FontStyleBold, UnitPixel);
	CheckCreateFont(&ui_context->hint_font, &ffEn, &ffEnDefault, fontHeight * 9 / 10, FontStyleRegular, UnitPixel);
	//ui_context->zi_font = new Font(&ffCn, fontHeight, FontStyleRegular, UnitPixel);
	//if(ui_context->zi_font->GetLastStatus()!=Ok){
	//	delete ui_context->zi_font;
	//	ui_context->zi_font = new Font(&ffCnDefault, fontHeight, FontStyleRegular, UnitPixel);
	//}
	//ui_context->no_font = new Font(&ffCn, fontHeight * 85 / 100, FontStyleRegular, UnitPixel);
	//if(ui_context->no_font->GetLastStatus()!=Ok){
	//	delete ui_context->no_font;
	//	ui_context->no_font = new Font(&ffCnDefault, fontHeight * 85 / 100, FontStyleRegular, UnitPixel);
	//}
	//ui_context->guide_font = new Font(&ffCn, fontHeight * 4 / 5, FontStyleRegular, UnitPixel);
	//if(ui_context->guide_font->GetLastStatus()!=Ok){
	//	delete ui_context->guide_font;
	//	ui_context->guide_font = new Font(&ffCnDefault, fontHeight * 4 / 5, FontStyleRegular, UnitPixel);
	//}

	//ui_context->ascii_font = new Font(&ffEn, fontHeight, FontStyleBold, UnitPixel);
	//if(ui_context->ascii_font->GetLastStatus()!=Ok){
	//	delete ui_context->ascii_font;
	//	ui_context->ascii_font = new Font(&ffEnDefault, fontHeight, FontStyleBold, UnitPixel);
	//}
	//ui_context->tone_font = new Font(&ffEn, fontHeight * 2 / 3, FontStyleBold, UnitPixel);
	//if(ui_context->tone_font->GetLastStatus()!=Ok){
	//	delete ui_context->tone_font;
	//	ui_context->tone_font = new Font(&ffEnDefault, fontHeight * 2 / 3, FontStyleBold, UnitPixel); 
	//}
	//ui_context->hint_font = new Font(&ffEn, fontHeight * 9 / 10, FontStyleRegular, UnitPixel);
	//if(ui_context->hint_font->GetLastStatus()!=Ok){
	//	delete ui_context->hint_font;
	//	ui_context->hint_font = new Font(&ffEnDefault, fontHeight * 9 / 10, FontStyleRegular, UnitPixel);
	//}

	//LOGFONT	lf;

	////候选字体
	//memset(&lf, 0, sizeof(lf));
	//lf.lfCharSet = GB2312_CHARSET;
	//lf.lfHeight	 = -pim_config->font_height;
	//lf.lfWeight  = FW_MEDIUM;
	//_tcscpy(lf.lfFaceName, pim_config->chinese_font_name);
	//ui_context->zi_font = CreateFontIndirect(&lf);

	//数字字体
	//lf.lfHeight = lf.lfHeight * 85 / 100;
	//ui_context->no_font = CreateFontIndirect(&lf);

	//英文字体
	//memset(&lf, 0, sizeof(lf));
	//lf.lfCharSet = DEFAULT_CHARSET;
	//lf.lfHeight  = -pim_config->font_height;// + 1;
	//lf.lfWeight  = pim_config->font_height >= 26 ? FW_MEDIUM : FW_SEMIBOLD;
	////lf.lfQuality = CLEARTYPE_QUALITY;
	//_tcscpy(lf.lfFaceName, pim_config->english_font_name);
	//ui_context->ascii_font = CreateFontIndirect(&lf);

	//构造音调字体
	//lf.lfHeight = lf.lfHeight * 2 / 3;
	//ui_context->tone_font = CreateFontIndirect(&lf);

	//构造提示字体
	//lf.lfWeight = FW_NORMAL;
	//lf.lfHeight = -pim_config->font_height + 1;
	//ui_context->hint_font = CreateFontIndirect(&lf);

	//构造功能提示字体
	//lf.lfCharSet = GB2312_CHARSET;
	//lf.lfWeight  = FW_NORMAL;
	//lf.lfHeight  = -pim_config->font_height + 4;
	//_tcscpy(lf.lfFaceName, pim_config->chinese_font_name);
	//ui_context->guide_font = CreateFontIndirect(&lf);
}

static void PIM_DestroyFonts(UICONTEXT *ui_context)
{
	if (!ui_context)
		return;

	delete ui_context->zi_font;
	delete ui_context->ascii_font;
	delete ui_context->hint_font;
	delete ui_context->tone_font;
	delete ui_context->no_font;
	delete ui_context->guide_font;

	ui_context->hint_font	= NULL;
	ui_context->ascii_font	= NULL;
	ui_context->tone_font	= NULL;
	ui_context->zi_font		= NULL;
	ui_context->no_font		= NULL;
	ui_context->guide_font	= NULL;
	//DeleteObject(ui_context->hint_font);
	//DeleteObject(ui_context->ascii_font);
	//DeleteObject(ui_context->tone_font);
	//DeleteObject(ui_context->zi_font);
	//DeleteObject(ui_context->no_font);
	//DeleteObject(ui_context->guide_font);

	//ui_context->hint_font	= 0;
	//ui_context->ascii_font	= 0;
	//ui_context->tone_font	= 0;
	//ui_context->zi_font		= 0;
	//ui_context->no_font		= 0;
	//ui_context->guide_font	= 0;
}

/*	依照当前状态，设置每一个按钮的图像索引
 */
int GetStatusButtonIconID(PIMCONTEXT *context, int button_id)
{
	switch(button_id)
	{
	case STATUS_BUTTON_MODE:			//输入模式按钮
		if (context->capital)
			return 2;

		if (context->input_mode & CHINESE_MODE)
			return 0;

		return 1;

	case STATUS_BUTTON_CHARSET:			//常用、全集开关按钮
		if (pim_config->hz_output_mode & HZ_OUTPUT_SIMPLIFIED)
			return 3;

		if (pim_config->hz_output_mode & HZ_OUTPUT_TRADITIONAL)
			return 4;

		return 5;

	case STATUS_BUTTON_SYMBOL:			//标点符号按钮
		if (context->capital)
			return 8;

		if ((context->input_mode & CHINESE_MODE) && (pim_config->hz_option & HZ_SYMBOL_CHINESE))
			return 7;

		return 8;

	case STATUS_BUTTON_SHAPE:			// 全半角符号按钮
		if (!(pim_config->hz_option & HZ_SYMBOL_HALFSHAPE))
			return 10;

		return 9;

	case STATUS_BUTTON_SOFTKBD:			//软键盘开关按钮
		if (context->soft_keyboard)
			return 12;

		return 11;

	case STATUS_BUTTON_SETUP:
		return 13;

	case STATUS_BUTTON_HELP:			//帮助按钮
		return 15;
	}

	return 0;
}

/**	绘制提示窗口
 */
void PaintHintWindow(PIMCONTEXT *context, UICONTEXT *ui_context, HDC dc)
{
	//Font *old_font = NULL;
	//Font *aFont = NULL;
	//HPEN  pen, old_pen;
	//HBRUSH brush, old_brush;
	TCHAR line[MAX_HINT_LENGTH];
	TCHAR *p, *pos;
	int  w, h, x, y;
	Graphics g(dc);
	SolidBrush brush(Color(GetRValue(pim_config->main_sp_hint_back_color),GetGValue(pim_config->main_sp_hint_back_color),GetBValue(pim_config->main_sp_hint_back_color)));
	Pen pen(Color(GetRValue(pim_config->main_sp_hint_border_color),GetGValue(pim_config->main_sp_hint_border_color),GetBValue(pim_config->main_sp_hint_border_color)));
	TESTCONTEXTS;

	//SetBkMode(dc, TRANSPARENT);
	//brush = CreateSolidBrush(pim_config->main_sp_hint_back_color);
	//pen   = CreatePen(PS_SOLID, 1, pim_config->main_sp_hint_border_color);
	//old_font  = (HFONT)SelectObject(dc, ui_context->hint_font);
	//aFont = ui_context->hint_font;
	//old_pen   = (HPEN)SelectObject(dc, pen);
	//old_brush = (HBRUSH)SelectObject(dc, brush);
	g.FillRectangle(&brush, 0, 0, ui_context->hint_window_size.cx-1, ui_context->hint_window_size.cy-1);
	g.DrawRectangle(&pen, 0, 0, ui_context->hint_window_size.cx-1, ui_context->hint_window_size.cy-1);
	//Rectangle(dc, 0, 0, ui_context->hint_window_size.cx, ui_context->hint_window_size.cy);
	x = HINT_TEXT_GAP;
	y = HINT_LINE_GAP;

	_tcscpy_s(line, _SizeOf(line), hint_string);
	p = _tcstok_s(line, TEXT("\n"), &pos);
	while(p)
	{
		//GetTextSize(dc, p, &w, &h);
		GetStringWidth(context, ui_context, p, (int)_tcslen(p), g, &w, &h, ui_context->hint_font, 0); 
		//TextOut(dc, x, y, p, (int)_tcslen(p));
		Draw_Text(g, x, y, p, ui_context->hint_font, COLORREF(0), (int)_tcslen(p), 0); 
		y += HINT_LINE_GAP + h;
		p = _tcstok_s(0, TEXT("\n"), &pos);
	}

	//SelectObject(dc, old_pen);
	//DeleteObject(pen);
	//SelectObject(dc, old_brush);
	//DeleteObject(brush);
	//SelectObject(dc, old_font);
	//hFont=old_font;
}

LRESULT WINAPI HintWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	HDC			dc;
	PAINTSTRUCT	paint_struct;
	PIMCONTEXT	*context;
	UICONTEXT	*ui_context;

	context = GetPIMContextByWindow(GetUIWindowHandle(window));
	ui_context = GetUIContextByWindow(GetUIWindowHandle(window));

	switch (message)
	{
	case WM_SHOWWINDOW:
		return 0;

	case WM_CREATE:
		return 0;

	case WM_DESTROY:
		return 0;

	case WM_PAINT:
		dc = BeginPaint(window, &paint_struct);
		PaintHintWindow(context, ui_context, dc);
		EndPaint(window, &paint_struct);

		return 1;
	}

	return DefWindowProc(window, message, wParam, lParam);
}

/*	创建Hint窗口
 */
int CreateHintWindow(UICONTEXT *ui_context)
{
	WNDCLASSEX	hint_class;
	int style = WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW;

	if (ui_context->hint_window != 0)				//主窗口已经被创建
		return 1;

	//注册主窗口类
	hint_class.cbSize			= sizeof(hint_class);
	hint_class.style			= IME_WINDOW_STYLE;
    hint_class.cbClsExtra		= 0;
    hint_class.cbWndExtra		= 0;
    hint_class.hIcon			= 0;
    hint_class.hIconSm			= 0;
	hint_class.hInstance		= global_instance;
    hint_class.hCursor			= 0;
    hint_class.hbrBackground	= 0;
    hint_class.lpszMenuName		= 0;
	hint_class.lpfnWndProc		= HintWindowProcedure;
	hint_class.lpszClassName	= HINT_WINDOW_CLASS_NAME;

	RegisterClassEx(&hint_class);

	//创建Hint窗口
	ui_context->hint_window = CreateWindowEx(
		style,
		HINT_WINDOW_CLASS_NAME,			//Class Name
		0,								//Window Name
		WS_POPUP | WS_DISABLED,			//Style
		100,							//Window position
		100,							//Window position
		400,							//Window size
		400,							//Window size
		//由于有遮挡的关系，所以必须将ui_window设置为父窗口，以后
		//再考虑使用其他方法解决。
		0, //ui_context->ui_window,		//Parent window
		0,								//Menu
		global_instance,				//Application instance
		0);								//Parameter

	if (ui_context->hint_window)
	{
		SetWindowLongPtr(ui_context->hint_window, GWLP_USERDATA, (__int3264)(LONG_PTR)ui_context->ui_window);

		RegisterHintWindow(ui_context->status_window, ui_context->hint_window);
	}

	return ui_context->hint_window ? 1 : 0;
}

void CreateStatusTooltipWindow(PIMCONTEXT *context, UICONTEXT *ui_context)
{
	int i;
	TOOLINFO tool_info;
	TCHAR input_mode_hint[MAX_HINT_LENGTH];
	TCHAR softkb_hint[MAX_HINT_LENGTH];
	TCHAR softkb_key[2] = {0};

	//由于Maxthon的多线程限制，使用ToolTip窗口会造成
	//对话框不能立即显示的错误，只好将这个功能放弃
	if (no_transparent)
		return;

	if (ui_context->tooltip_window)
		DestroyWindow(ui_context->tooltip_window);

	ui_context->tooltip_window = CreateWindowEx(	//创建状态窗口的ToolTip窗口
			WS_EX_TOOLWINDOW,
			TOOLTIPS_CLASS,
			0,
			WS_POPUP | WS_DISABLED | TTS_ALWAYSTIP | TTS_NOPREFIX,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			ui_context->status_window,
			0,
			global_instance,
			0);

	if (!ui_context->tooltip_window)
		return;

	SendMessage(ui_context->tooltip_window, TTM_SETDELAYTIME, TTDT_AUTOPOP, TOOLTIP_DELAYTIME_AUTOPOP);
	SendMessage(ui_context->tooltip_window, TTM_SETDELAYTIME, TTDT_INITIAL, TOOLTIP_DELAYTIME_INITIAL);
	SendMessage(ui_context->tooltip_window, TTM_SETDELAYTIME, TTDT_RESHOW,  TOOLTIP_DELAYTIME_RESHOW);

	tool_info.cbSize = sizeof(TOOLINFO);
	tool_info.uFlags = 0;
	tool_info.hwnd   = ui_context->status_window;
	tool_info.hinst  = global_instance;

	//逐个设置每一个Tip信息
	for (i = 0; i < status_button_count; i++)
	{
		tool_info.uId         = TOOLTIP_BASE_ID + i;
		tool_info.rect.left   = status_button_pos[i].x;
		tool_info.rect.top    = status_button_pos[i].y;
		tool_info.rect.right  = tool_info.rect.left + status_button_width;
		tool_info.rect.bottom = tool_info.rect.top + status_button_height;

		switch (status_button_hint[status_button_id[i]].item_type)
		{
			case STATUS_BUTTON_HINT_INPUT_MODE:
				_tcscpy_s(input_mode_hint, MAX_HINT_LENGTH, status_button_hint[status_button_id[i]].item_text);

				if (KEY_SWITCH_SHIFT == pim_config->key_change_mode)
					_tcscat_s(input_mode_hint, MAX_HINT_LENGTH, TEXT("(Shift)"));
				else if (KEY_SWITCH_CONTROL == pim_config->key_change_mode)
					_tcscat_s(input_mode_hint, MAX_HINT_LENGTH, TEXT("(Ctrl)"));

				tool_info.lpszText = input_mode_hint;
				break;

			case STATUS_BUTTON_HINT_SOFTKEYBOARD:
				_tcscpy_s(softkb_hint, MAX_HINT_LENGTH, status_button_hint[status_button_id[i]].item_text);

				if (pim_config->use_key_soft_kbd)
				{
					softkb_key[0] = pim_config->key_soft_kbd;
					_tcscat_s(softkb_hint, MAX_HINT_LENGTH, TEXT("(Ctrl+Shift+"));
					_tcscat_s(softkb_hint, MAX_HINT_LENGTH, (TCHAR *)softkb_key);
					_tcscat_s(softkb_hint, MAX_HINT_LENGTH, TEXT(")"));
				}

				tool_info.lpszText = softkb_hint;
				break;

			default:
				tool_info.lpszText = (LPTSTR)status_button_hint[status_button_id[i]].item_text;
		}

		SendMessage(ui_context->tooltip_window, TTM_ADDTOOL, 0, (LPARAM)((LPTOOLINFO)&tool_info));
	}
}

void SetStatusWindowPosition(UICONTEXT *ui_context)
{
	RECT monitor_rect, window_rect;
	int  last_x, last_y;
	POINT point;

	if (!pim_config || !ui_context)
		return;

	last_x = pim_config->status_window_x;
	last_y = pim_config->status_window_y;

	if (last_x == -1 && last_y == -1)		//第一次进行窗口位置设定
	{
		point.x = point.y = 0;
		monitor_rect = GetMonitorRectFromPoint(point);
		pim_config->status_window_x = monitor_rect.right - status_window_width;
		pim_config->status_window_y = monitor_rect.bottom - status_window_height;
	}
	else
	{
		point.x = window_rect.left = pim_config->status_window_x;
		point.y = window_rect.top = pim_config->status_window_y;
		window_rect.right  = window_rect.left + status_window_width;
		window_rect.bottom = window_rect.top + status_window_height;
		monitor_rect = GetMonitorRectFromPoint(point);
		MakeRectInRect(&window_rect, monitor_rect);

		pim_config->status_window_x = window_rect.left;
		pim_config->status_window_y = window_rect.top;
	}

	MoveWindow(ui_context->status_window,
				 pim_config->status_window_x,
				 pim_config->status_window_y,
				 status_window_width,
				 status_window_height,
				 0);

	pim_config->status_window_x = last_x;
	pim_config->status_window_y = last_y;

	if (last_x != pim_config->status_window_x || last_y != pim_config->status_window_y)
	{
		Log(LOG_ID, L"记录状态窗口位置");
		SaveConfigInternal(pim_config);			//保存状态窗口位置
	}
}

/*	计算并设置StatusWindow上button位置，以及ToolTip的内容与区域
 */
void PreShowStatusWindow(PIMCONTEXT *context, UICONTEXT *ui_context)
{
	int i;

	if (!context || !ui_context)
		return;

	Bitmap *image_status_buttons = ui_context->image_status_buttons;
	Bitmap *image_status_bk		 = ui_context->image_status_bk;

	//如果与上一次的情况一样，则不需要进行计算
	if (status_buttons == pim_config->status_buttons)
		return;

	status_buttons = pim_config->status_buttons;
	status_button_count = 0;
	for (i = 0; i < MAX_STATUS_BUTTON_NUMBER; i++)
		if (status_buttons & (1 << i))
			status_button_id[status_button_count++] = i;

	//状态窗口的高度为背景高度
	//宽度为按钮图片一个元素的宽度*按钮数目+OFFSET
	if (image_status_buttons)		//有图像的时候才对
	{
		status_button_width  = image_status_buttons->GetWidth() / 9;
		status_button_height = image_status_buttons->GetHeight() / 2;
	}
	else
	{
		status_button_width  = STATUS_BUTTON_WIDTH;
		status_button_height = STATUS_BUTTON_HEIGHT;
	}

	status_window_width  = status_button_width * status_button_count + pim_config->status_left_space + pim_config->status_right_space;

	if (status_window_width < pim_config->status_min_width)
		status_window_width = pim_config->status_min_width;

	if (image_status_bk)
		status_window_height = image_status_bk->GetHeight();
	else
		status_window_height = status_button_height;

	for (i = 0; i < status_button_count; i++)
	{
		status_button_pos[i].x = pim_config->status_left_space + i * status_button_width;
		if (pim_config->status_window_top_margin == -1)		//居中
			status_button_pos[i].y = status_window_height / 2 - status_button_height / 2;
		else
			status_button_pos[i].y = pim_config->status_window_top_margin;
	}

	CreateStatusTooltipWindow(context, ui_context);
}

void StatusDrawDownFrame(UICONTEXT *ui_context, Graphics *g, int x, int y)
{
	if (!ui_context)
		return;

	Bitmap *image_status_buttons = ui_context->image_status_buttons;
	g->DrawImage(image_status_buttons,
				Rect(x, y + 1, status_button_width, status_button_height - 2),
				status_button_width * 8,
				status_button_height + 1,
				status_button_width,
				status_button_height - 2,
				UnitPixel);
}

void StatusDrawOverFrame(UICONTEXT *ui_context, Graphics *g, int x, int y)
{
	if (!ui_context)
		return;

	Bitmap *image_status_buttons = ui_context->image_status_buttons;
	g->DrawImage(image_status_buttons,
				Rect(x, y + 1, status_button_width, status_button_height - 2),
				status_button_width * 7,
				status_button_height + 1,
				status_button_width,
				status_button_height - 2,
				UnitPixel);
}

#define	STATUS_MOUSE_MOVE			1
#define	STATUS_MOUSE_CLICK			2

/*	画出按钮图标。
 *	参数：
 *		dc					画布
 *		index				按钮序号
 *		icon_id				图标标识
 *		flag				特殊效果：鼠标悬浮、鼠标点击
 */
void StatusDrawButton(UICONTEXT *ui_context, Graphics *g, int index, int icon_id, int flag)
{
	int x, y;

	if (!ui_context)
		return;

	Bitmap *image_status_buttons = ui_context->image_status_buttons;

	x = status_button_pos[index].x;
	y = status_button_pos[index].y;

	if (flag & STATUS_MOUSE_MOVE)			//悬浮，需要画出外框
		StatusDrawOverFrame(ui_context, g, x, y);

	if (flag & STATUS_MOUSE_CLICK)			//鼠标点击
		StatusDrawDownFrame(ui_context, g, x, y);

	int dx = (icon_id % 9) * status_button_width;
	int dy = (icon_id / 9) * status_button_height;

	g->DrawImage(image_status_buttons,
				Rect(x, y, status_button_width, status_button_height),
				dx, dy, status_button_width, status_button_height,
				UnitPixel, 0, 0, 0);
}

/**	绘制状态窗口的背景图像
 */
void DrawStatusBackground(UICONTEXT *ui_context, Graphics *g,int alpha)
{
	if (!ui_context)
		return;

	SolidBrush brush(Color(alpha,255,254,255));
	g->FillRectangle(&brush, 0, 0, status_window_width, status_window_height);

	ImageAttributes	image_attribute;
	image_attribute.SetColorKey(Color(255, 254, 255), Color(255, 254, 255), ColorAdjustTypeBitmap);

	Bitmap *image_status_bk = ui_context->image_status_bk;
	if (!image_status_bk)
		return;

	//校验参数
	if (pim_config->status_left_margin < 0 || pim_config->status_left_margin > (int)image_status_bk->GetWidth() ||
		pim_config->status_right_margin < 0 || pim_config->status_right_margin > (int)image_status_bk->GetWidth() ||
		pim_config->status_left_margin + pim_config->status_right_margin > (int)image_status_bk->GetWidth())
		return;

	//绘制左边
	g->DrawImage(image_status_bk,
		Rect(0, 0, pim_config->status_left_margin, status_window_height),
		0, 0, pim_config->status_left_margin, status_window_height,
		UnitPixel);

	//绘制右边
	g->DrawImage(image_status_bk,
		Rect(status_window_width - pim_config->status_right_margin, 0, pim_config->status_right_margin, status_window_height),
		image_status_bk->GetWidth() - pim_config->status_right_margin, 0, pim_config->status_right_margin, image_status_bk->GetHeight(),
		UnitPixel,
		&image_attribute);

	//绘制中心：拉伸模式
	if (pim_config->status_center_mode == DRAW_MODE_STRETCH)
	{
		g->DrawImage(image_status_bk,
			Rect(pim_config->status_left_margin, 0, status_window_width - pim_config->status_right_margin - pim_config->status_left_margin, status_window_height),
			pim_config->status_left_margin, 0, image_status_bk->GetWidth() - pim_config->status_left_margin - pim_config->status_right_margin, image_status_bk->GetHeight(),
			UnitPixel,
			&image_attribute);
	}
	else	//平铺模式
	{
		int tag_width  = status_window_width - pim_config->status_left_margin - pim_config->status_right_margin;
		int tag_height = status_window_height;
		int src_width  = image_status_bk->GetWidth() - pim_config->status_left_margin - pim_config->status_right_margin;
		int src_height = image_status_bk->GetHeight();

		image_attribute.SetWrapMode(WrapModeTile);
		Bitmap *center_image = image_status_bk->Clone(pim_config->status_left_margin, 0, src_width, src_height, PixelFormatDontCare);
		g->DrawImage(center_image,
			Rect(pim_config->status_left_margin, 0, tag_width, tag_height),
			0, 0, tag_width, tag_height,
			UnitPixel,
			&image_attribute);
		delete center_image;
	}

	DrawStatusAssistLine(ui_context, g);
}

/**	绘制背景窗口
 */
void PaintStatusWindow(PIMCONTEXT *context, UICONTEXT *ui_context, HDC dc)
{
	HWND hWnd = WindowFromDC(dc);
	RECT rect;
	GetWindowRect(hWnd,&rect);

	int	i, icon_id;
	POINT pt  = {0, 0};
	POINT pos = {rect.left, rect.top};
	HBITMAP	bitmap = 0;

	//当size超过bmp的大小的时候，ULW函数不报错，但没有任何动作。
	SIZE size = {rect.right - rect.left, rect.bottom - rect.top};

	//Alpha通道窗口必须每次都要设置EX_STYLE，否则出错！
	SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd,GWL_EXSTYLE) | WS_EX_LAYERED);

	HDC m_dc = CreateCompatibleDC(dc);
	bitmap	 = GetHBitMapFromRect(rect.right - rect.left, rect.bottom - rect.top);
	SelectObject(m_dc, bitmap);

	Graphics g(m_dc);
	DrawStatusBackground(ui_context, &g, 0);

	//画按钮
	for (i = 0; i < status_button_count; i++)
	{
		int flag = 0;

		if (i == current_down_button_index)
			flag |= STATUS_MOUSE_CLICK;
		if (i == current_move_button_index)
			flag |= STATUS_MOUSE_MOVE;

		icon_id = GetStatusButtonIconID(context, 1 << status_button_id[i]);
		StatusDrawButton(ui_context, &g, i, icon_id, flag);
	}

	BLENDFUNCTION blend_function = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

	UpdateLayeredWindow(hWnd, 0, &pos, &size, m_dc, &pt, 0, &blend_function, ULW_ALPHA);

	DeleteObject(m_dc);
	DeleteObject(bitmap);
}

/**	设置界面/主题制作:绘制背景窗口
 */
void PaintStatusCanvas(PIMCONTEXT *context, UICONTEXT *ui_context, HDC dc)
{
	int		i, icon_id;
	Color	color;
	HDC		memory_dc;
	HBITMAP	bitmap = 0;

	memory_dc = CreateCompatibleDC(dc);
	bitmap = CreateCompatibleBitmap(dc, status_window_width, status_window_height);
	SelectObject(memory_dc, bitmap);

	Graphics g(memory_dc);

	DrawStatusBackground(ui_context, &g, 254);

	//画按钮
	for (i = 0; i < status_button_count; i++)
	{
		int flag = 0;

		if (i == current_down_button_index)
			flag |= STATUS_MOUSE_CLICK;
		if (i == current_move_button_index)
			flag |= STATUS_MOUSE_MOVE;

		icon_id = GetStatusButtonIconID(context, 1 << status_button_id[i]);
		StatusDrawButton(ui_context, &g, i, icon_id, flag);
	}

	g.Flush();

	BitBlt(dc, 0, 0, status_window_width, status_window_height, memory_dc, 0, 0, SRCCOPY);

	DeleteObject(memory_dc);
	DeleteObject(bitmap);
}

int GetStatusCursorZone(UICONTEXT *ui_context, int *index)
{
	int i;
	POINT cursor_pos;					//光标在屏幕以及窗口中的位置

	GetCursorPos(&cursor_pos);
	ScreenToClient(ui_context->status_window, &cursor_pos);

	if (cursor_pos.x >= 0 && cursor_pos.x <= pim_config->status_left_space)
		return ZONE_DRAG;

	if (cursor_pos.x >= status_window_width - pim_config->status_right_space && cursor_pos.x <= status_window_width)
		return ZONE_DRAG;

	*index = 0;
	if (cursor_pos.y < status_button_pos[0].y || cursor_pos.y > status_button_pos[0].y + status_button_height)
		return ZONE_DRAG;

	for (i = 0; i < status_button_count; i++)
		if (cursor_pos.x >= status_button_pos[i].x && cursor_pos.x <= status_button_pos[i].x + status_button_width)
		{
			*index = i;
			return ZONE_BUTTON;
		}

	return ZONE_DRAG;
}

void PassToToolTipWindow(UICONTEXT *ui_context, HWND owner_window, UINT message)
{
	MSG		msg;
	POINT	cursor_pos;

	msg.hwnd	= owner_window;
	msg.message = message;
	msg.wParam	= 0;

	GetCursorPos(&cursor_pos);
	ScreenToClient(owner_window, &cursor_pos);
	msg.lParam = MAKELONG(cursor_pos.x, cursor_pos.y);
	SendMessage(ui_context->tooltip_window, TTM_RELAYEVENT, 0, (LPARAM) (LPMSG) &msg);
}

/**	打开Help文档
 */
void OpenHelp()
{
	Log(LOG_ID, L"打开Help文档");

#ifdef	_VER_SIFA_
	TCHAR install_dir[MAX_PATH];

	if (!GetInstallDir(install_dir))
		return;

	_tcscat_s(install_dir, _SizeOf(install_dir), TEXT("\\doc\\help\\index.html"));

	ExecuateProgram(install_dir, 0);
	return;
#else
	ExecuateProgram(TEXT("http://www.unispim.com/help/index.html"), 0, 1);
	return;
#endif
}

void ClickedButton(PIMCONTEXT *context, UICONTEXT *ui_context, int index)
{
	int button_name;

	if (index == -1)
		return;

	button_name = 1 << status_button_id[index];
	Log(LOG_ID, L"状态窗口按钮点击, id=%d", button_name);

	switch(button_name)
	{
	case STATUS_BUTTON_MODE:		//英文中文切换
		ToggleChineseMode(context);
		PostMessage(ui_context->ui_window, UIM_MODE_CHANGE, 0, 0);
		break;

	case STATUS_BUTTON_CHARSET:		//常用、全集开关按钮
		ToggleFanJian(context);
		PostMessage(ui_context->ui_window, UIM_MODE_CHANGE, 0, 0);
		break;

	case STATUS_BUTTON_SYMBOL:		//标点符号按钮
		ToggleEnglishSymbol(context);
		PostMessage(ui_context->ui_window, UIM_MODE_CHANGE, 0, 0);
		break;

	case STATUS_BUTTON_SHAPE:		//全半角符号按钮
		ToggleFullShape(context);
		PostMessage(ui_context->ui_window, UIM_MODE_CHANGE, 0, 0);
		break;

	case STATUS_BUTTON_SOFTKBD:		//软键盘开关按钮
		context->soft_keyboard = !context->soft_keyboard;
		SaveConfigInternal(pim_config);
		PostMessage(ui_context->ui_window, UIM_MODE_CHANGE, 0, 0);
		break;

	case STATUS_BUTTON_SETUP:		//配置工具按钮
		if (window_logon)
			break;

		HideStatusWindow(context, ui_context);
		ImeConfigure(0, 0, 1, 0);
		break;

	case STATUS_BUTTON_HELP:		//帮助按钮
		if (window_logon)
			break;

		OpenHelp();
		break;
	}
}

/*	刷新状态窗口的位置
 */
void RefreshStatusWindowPosition(UICONTEXT *ui_context)
{
	RECT rect;
	if (!pim_config)
		return;

	GetWindowRect(ui_context->status_window, &rect);
	pim_config->status_window_x = rect.left;
	pim_config->status_window_y = rect.top;
	SaveConfigInternal(pim_config);
}

/*	检查菜单状态
 */
void CheckMenu(PIMCONTEXT *context, HMENU menu)
{
	if (pim_config->trace_caret)
		CheckMenuItem(menu, ID_GBGS, MF_CHECKED);

	if (context->soft_keyboard)
		CheckMenuItem(menu, ID_SKB0 + context->softkbd_index, MF_CHECKED);

	if (pim_config->hz_output_mode & HZ_OUTPUT_HANZI_ALL)
		CheckMenuItem(menu, ID_HZ_ALL, MF_CHECKED);
	else if (pim_config->hz_output_mode & HZ_OUTPUT_TRADITIONAL)
		CheckMenuItem(menu, ID_FANTI, MF_CHECKED);
	else if (pim_config->hz_output_mode & HZ_OUTPUT_SIMPLIFIED)
		CheckMenuItem(menu, ID_JIANTI, MF_CHECKED);

	if (pim_config->pinyin_mode == PINYIN_SHUANGPIN)
		CheckMenuItem(menu, ID_SHUANGPIN, MF_CHECKED);
	else
		CheckMenuItem(menu, ID_QUANPIN, MF_CHECKED);

	if (ENGLISH_STATE_INPUT == context->english_state)
		CheckMenuItem(menu, ID_ENGLISH_INPUT, MF_CHECKED);

	if (pim_config->show_status_window)
		CheckMenuItem(menu, ID_SHOWSTATUS, MF_CHECKED);
}

TCHAR theme_list[MAX_THEME_COUNT][256] =
{
	TEXT("0"),	TEXT("1"),	TEXT("2"),	TEXT("3"),	TEXT("4"),	TEXT("5"),	TEXT("6"),	TEXT("7"),
	TEXT("8"),	TEXT("9"),	TEXT("a"),	TEXT("b"),	TEXT("c"),	TEXT("d"),	TEXT("e"),	TEXT("f"),
};

//制作主题的函数名
typedef int (_stdcall *pMakeThemeList) (TCHAR [MAX_THEME_COUNT][256]);

void InsertThemeMenu(HMENU menu)
{
	HMENU theme_menu;
	int i, theme_count;
	TCHAR *cur_theme_name = pim_config->theme_name;

	theme_count = GetThemeList(theme_list);
	theme_menu = GetSubMenu(menu, 0);

	for (i = 0; i < theme_count; i++)
	{
		AppendMenu(theme_menu, MF_STRING, ID_SKIN_BASE + i, theme_list[i]);
		if (!_tcscmp(theme_list[i], cur_theme_name))
			CheckMenuItem(theme_menu, ID_SKIN_BASE + i, MF_CHECKED);
	}
}

#define	MENU_BUTTONS_NAME	TEXT("unispim6\\theme\\menu_buttons.bmp")
#define	MENU_BUTTON_NUMBER	2
#define	MENU_BUTTON_WIDTH	12
#define	MENU_BUTTON_HEIGHT	12

/**	设置或清除菜单左边的按钮
 *	参数：
 *		is_set		是否设定按钮的图标
 */
void ProcessMenuButtons(int is_set, HMENU menu)
{
	static HBITMAP h_bitmap[MENU_BUTTON_NUMBER] = {0};
	static Bitmap  *bitmap[MENU_BUTTON_NUMBER] = {0};
	static Bitmap  *image = 0;
	//HMENU popup_menu;
	extern Bitmap  *LoadImageResource(const TCHAR *image_name);

	if (!image)
	{
		image = LoadImageResource(MENU_BUTTONS_NAME);
		if (!image)
			return;

		for (int i = 0; i < MENU_BUTTON_NUMBER; i++)
		{
			bitmap[i] = image->Clone(Rect(i * MENU_BUTTON_WIDTH, 0, MENU_BUTTON_WIDTH, MENU_BUTTON_HEIGHT), PixelFormatDontCare);
			bitmap[i]->GetHBITMAP(0, &h_bitmap[i]);
		}
	}

	SetMenuItemBitmaps(menu,  1, MF_BYPOSITION, h_bitmap[0], 0);
	if (!pim_config->use_english_input)
		SetMenuItemBitmaps(menu, 13, MF_BYPOSITION, h_bitmap[1], 0);
	else
		SetMenuItemBitmaps(menu, 14, MF_BYPOSITION, h_bitmap[1], 0);
	//SetMenuItemBitmaps(menu,  3, MF_BYPOSITION, h_bitmap[2], 0);
	//SetMenuItemBitmaps(menu,  4, MF_BYPOSITION, h_bitmap[4], 0);
	//SetMenuItemBitmaps(menu,  6, MF_BYPOSITION, h_bitmap[3], 0);
	//SetMenuItemBitmaps(menu,  7, MF_BYPOSITION, h_bitmap[6], 0);

	//popup_menu = GetSubMenu(menu, 4);

	//SetMenuItemBitmaps(popup_menu, 0, MF_BYPOSITION, h_bitmap[12], 0);	//config wizard
	//SetMenuItemBitmaps(popup_menu, 1, MF_BYPOSITION, h_bitmap[7],  0);	//ime manager
	//SetMenuItemBitmaps(popup_menu, 2, MF_BYPOSITION, h_bitmap[5],  0);	//radical
	//SetMenuItemBitmaps(popup_menu, 3, MF_BYPOSITION, h_bitmap[9],  0);	//czsr
	//SetMenuItemBitmaps(popup_menu, 4, MF_BYPOSITION, h_bitmap[10], 0);	//spw
	//SetMenuItemBitmaps(popup_menu, 5, MF_BYPOSITION, h_bitmap[11], 0);	//theme maker
	//SetMenuItemBitmaps(popup_menu, 6, MF_BYPOSITION, h_bitmap[13], 0);	//new word
	//SetMenuItemBitmaps(popup_menu, 7, MF_BYPOSITION, h_bitmap[8],  0);	//batch word
	//SetMenuItemBitmaps(popup_menu, 8, MF_BYPOSITION, h_bitmap[14], 0);	//zi manager
	//SetMenuItemBitmaps(popup_menu, 9, MF_BYPOSITION, h_bitmap[15], 0);	//zi manager
	//SetMenuItemBitmaps(popup_menu, 10, MF_BYPOSITION, h_bitmap[16], 0);//font search
}

void SetMenuShortcut(HMENU hMenu, UINT uItem, BOOL fByPosition, UINT uKey)
{
	MENUITEMINFO info;
	TCHAR menu_text[255] = {0};
	TCHAR short_cut_text[255] = {0};

	info.cbSize		= sizeof(MENUITEMINFO);
	info.fMask		= MIIM_STRING;
	info.dwTypeData = menu_text;
	info.cch		= _SizeOf(menu_text) - 1;

	GetMenuItemInfo(hMenu, uItem, fByPosition, &info);

	_stprintf_s(short_cut_text, _SizeOf(short_cut_text), TEXT("(Ctrl+Shift+%c)"), uKey);
	_tcscat_s(menu_text, _SizeOf(menu_text), short_cut_text);

	SetMenuItemInfo(hMenu, uItem, fByPosition, &info);
}

/**	处理菜单快捷键
 */
void ProcessMenuStuff(HMENU menu)
{
	//HMENU quick_menu;

	//软键盘快捷键
	if (pim_config->use_key_soft_kbd)
		SetMenuShortcut(menu, 1, 1, pim_config->key_soft_kbd);

	//快速设置菜单
	//quick_menu = GetSubMenu(menu, 2);

	//不使用英文输入法，删掉切换英文输入法的菜单
	if (!pim_config->use_english_input)
	{
		DeleteMenu(menu, 3, MF_BYPOSITION);
		DeleteMenu(menu, ID_ENGLISH_INPUT, MF_BYCOMMAND);
	}

	if (pim_config->use_key_quan_shuang_pin || pim_config->use_key_jian_fan ||
		pim_config->use_key_status_window || pim_config->use_key_english_input)
	{
		//全拼、双拼
		if (pim_config->use_key_quan_shuang_pin)
		{
			SetMenuShortcut(menu, ID_QUANPIN, 0, pim_config->key_quan_shuang_pin);
			SetMenuShortcut(menu, ID_SHUANGPIN, 0, pim_config->key_quan_shuang_pin);
		}

		//简体、繁体、全集
		if (pim_config->use_key_jian_fan)
		{
			SetMenuShortcut(menu, ID_JIANTI, 0, pim_config->key_jian_fan);
			SetMenuShortcut(menu, ID_FANTI, 0, pim_config->key_jian_fan);
			SetMenuShortcut(menu, ID_HZ_ALL, 0, pim_config->key_jian_fan);
		}

		//状态窗口
		if (pim_config->use_key_status_window)
			SetMenuShortcut(menu, ID_SHOWSTATUS, 0, pim_config->key_status_window);

		//英文输入法
		if (pim_config->use_english_input && pim_config->use_key_english_input)
			SetMenuShortcut(menu, ID_ENGLISH_INPUT, 0, pim_config->key_english_input);
	}
}

/*	显示菜单
 */
int TrackMenu(HWND hwnd, PIMCONTEXT *context, UICONTEXT *ui_context, BOOL status_window)
{
	int zone, index;
	int softkbd = 0;
	int item_id, button_id;
	UINT uFlags;
	RECT  rect;
	HMENU menu, popup_menu;
	POINT cursor_pos, popup_pos;

	GetCursorPos(&cursor_pos);

	if (status_window)
	{
		zone = GetStatusCursorZone(ui_context, &index);
		if (zone == ZONE_BUTTON)
		{
			button_id = 1 << status_button_id[index];
			if (button_id == STATUS_BUTTON_SOFTKBD)
				softkbd = 1;
		}
	}

	menu = LoadMenu(global_instance, MAKEINTRESOURCE(IDR_IMEMENU));

	popup_menu = GetSubMenu(menu, 0);

//#ifdef	_VER_SIFA_
	//DeleteMenu(popup_menu, 3, MF_BYPOSITION);
//#else
	//DeleteMenu(popup_menu, 4, MF_BYPOSITION);
//#endif

	ProcessMenuStuff(popup_menu);

	ProcessMenuButtons(1, popup_menu);

	InsertThemeMenu(popup_menu);
//	InsertHelpMenu(popup_menu);

	CheckMenu(context, popup_menu);

	if (status_window)
	{
		if (softkbd)
			popup_menu = GetSubMenu(popup_menu, 1);

		GetWindowRect(hwnd, &rect);

		popup_pos.x = rect.right;
		popup_pos.y = rect.top;

		uFlags = TPM_LEFTBUTTON | TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RETURNCMD;
	}
	else
	{
		popup_pos.x = cursor_pos.x;
		popup_pos.y = cursor_pos.y;

		uFlags = TPM_LEFTBUTTON | TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RETURNCMD;
	}

	ui_context->menu_showing = 1;
	SetCursor(LoadCursor(0, IDC_ARROW));
	item_id = TrackPopupMenu(popup_menu, uFlags,
							 popup_pos.x, popup_pos.y, 0,
							 hwnd,
							 0);
	ui_context->menu_showing = 0;

	//有特殊用处？
	PostMessage(hwnd, WM_NULL, 0, 0);	//see the msdn remark

	ProcessMenuButtons(0, popup_menu);
	DestroyMenu(menu);

	//弹出菜单有可能遮挡写作窗口，因此重新显示一下
	if (IsWindowVisible(ui_context->main_window))
		ShowMainWindow(context, ui_context);

	return item_id;
}

/*	处理菜单项
 */
void ProcessMenu(PIMCONTEXT *context, UICONTEXT *ui_context, int id)
{
	Log(LOG_ID, L"处理菜单, id = 0x%x", id);
	if (id >= ID_SKIN_BASE && id < ID_SKIN_MAX)
	{
		if (!_tcscmp(pim_config->theme_name, theme_list[id - ID_SKIN_BASE]))
			return;

		_tcscpy_s(pim_config->theme_name, _SizeOf(pim_config->theme_name), theme_list[id - ID_SKIN_BASE]);
		Log(LOG_ID, L"设置新主题:%s", pim_config->theme_name);
		LoadThemeConfig(pim_config);
		SaveConfigInternal(pim_config);
		share_segment->global_config_update_time = GetCurrentTicks();
		UpdateWinResource(context, context->ui_context);
		PostMessage(ui_context->ui_window, UIM_MODE_CHANGE, 0, 0);
		return;
	}

	switch(id)
	{
	case ID_CFG_WIZARD:                 //配置向导
		if (!window_logon)
			RunCFGWizard(ui_context->ui_window);

		break;

	case ID_WORD_BATCH:                 //批量造词
		if (!window_logon)
			RunBatchWords(ui_context->ui_window);

		break;

	case ID_ZI_MANAGE:                  //汉字管理
		if (!window_logon)
			RunZiEditor(ui_context->ui_window);

		break;

	case ID_FONT_SEARCH:                //字体检测
		break;

	case ID_THEME_MAKER:                //主题管理
		if (!window_logon)
			RunThemeMaker(ui_context->ui_window);

		break;

	//case ID_URL_MANAGE:                 //网址管理
	//	if (!window_logon)
	//		RunURLManager(ui_context->ui_window);

		break;

	case ID_SKIN_BROWSER:
		if (!window_logon)				//在系统登录时不能使用
			ExecuateProgram(TEXT("http://www.unispim.com/theme"), 0, 1);

		break;

	case ID_PIANPANGJIANZI:
		if (!window_logon)				//在系统登录时不能使用部首检字功能
			RunPianPangJianZi(ui_context->ui_window);

		break;

	case ID_SPWPLUGIN:
		if (!window_logon)				//在系统登录时不能使用spw plugin
			RunSPWPlugin(ui_context->ui_window);

		break;

	case ID_COMPOSE:
		if (!window_logon)				//在系统登录时不能使用拆拼组字功能
			RunCompose(ui_context->ui_window);

		break;

	case ID_ADDWORD:
		if (!window_logon)
			RunConfigWordlib();

		break;

	case ID_SETUP:
		if (!window_logon)				//在系统登录时不能使用Help
			ImeConfigure(0, 0, 1, 0);

		break;

	/*case ID_IMEMANAGER:
		if (!window_logon)
			RunImeManager(0);

		break;
*/
	case ID_HZ_ALL:
		Log(LOG_ID, L"全集");
		pim_config->hz_output_mode = HZ_OUTPUT_HANZI_ALL;
		SaveConfigInternal(pim_config);
		PostMessage(ui_context->ui_window, UIM_MODE_CHANGE, 0, 0);
		break;

	case ID_JIANTI:
		Log(LOG_ID, L"简体");
		pim_config->hz_output_mode = HZ_OUTPUT_SIMPLIFIED;
		SaveConfigInternal(pim_config);
		PostMessage(ui_context->ui_window, UIM_MODE_CHANGE, 0, 0);
		break;

	case ID_FANTI:
		Log(LOG_ID, L"繁体");
		pim_config->hz_output_mode = HZ_OUTPUT_TRADITIONAL;
		SaveConfigInternal(pim_config);
		PostMessage(ui_context->ui_window, UIM_MODE_CHANGE, 0, 0);
		break;

	case ID_SKB0:	case ID_SKB1:	case ID_SKB2:	case ID_SKB3:
	case ID_SKB4:	case ID_SKB5:	case ID_SKB6:	case ID_SKB7:
	case ID_SKB8:	case ID_SKB9:	case ID_SKB10:	case ID_SKB11:
	case ID_SKB12:	case ID_SKB13:	case ID_SKB14:
		context->soft_keyboard = 1;
		context->softkbd_index = id - ID_SKB0;
		Log(LOG_ID, L"软键盘:%d", context->softkbd_index);
		pim_config->soft_kbd_index = id - ID_SKB0;
		SaveConfigInternal(pim_config);
		PostMessage(ui_context->ui_window, UIM_MODE_CHANGE, 0, 0);
		break;

	case ID_VERSION:
		if (!window_logon)				//在系统登录时不能使用Help
			RunConfigVersion();
		return;

	//光标跟随
	case ID_GBGS:
		pim_config->trace_caret = !pim_config->trace_caret;
		Log(LOG_ID, L"光标跟随:%d", pim_config->trace_caret);
		SaveConfigInternal(pim_config);
		PostMessage(ui_context->ui_window, UIM_MODE_CHANGE, 0, 0);
		break;

	case ID_SHUANGPIN:
		Log(LOG_ID, L"双拼");
		pim_config->pinyin_mode = PINYIN_SHUANGPIN;
		SaveConfigInternal(pim_config);
		PostMessage(ui_context->ui_window, UIM_MODE_CHANGE, 0, 0);
		break;

	case ID_QUANPIN:
		Log(LOG_ID, L"全拼");
		pim_config->pinyin_mode = PINYIN_QUANPIN;
		SaveConfigInternal(pim_config);
		PostMessage(ui_context->ui_window, UIM_MODE_CHANGE, 0, 0);
		break;

	case ID_SHOWSTATUS:
		ToggleShowStatusWindow(context);
		break;

	case ID_ENGLISH_INPUT:
		Log(LOG_ID, L"英文输入法:%d", context->english_state);
		if (pim_config->use_english_input)
			ToggleEnglishInput(context);

		break;

	//禁止组合键
	case ID_DISABLECK:
		pim_config->disable_combined_key = !pim_config->disable_combined_key;
		SaveConfigInternal(pim_config);
		break;

	case ID_HELP_SF:
	case ID_HELPDIR:
	case ID_HELPINDEX:
		if (!window_logon)				//在系统登录时不能使用Help
			OpenHelp();

		break;
	}
}

#define	HINT_MAIN_OFFSET	16

/**	设置Hint窗口位置
 */
void SetHintWindowPos(UICONTEXT *ui_context)
{
	RECT monitor_rect;
	RECT window_rect;

	monitor_rect = GetMonitorRectFromPoint(ui_context->caret_pos);

	if (pim_config->trace_caret)		//光标跟随
	{
		window_rect.left = ui_context->caret_pos.x + HINT_MAIN_OFFSET;
		window_rect.top  = ui_context->caret_pos.y + CARET_HINT_Y_OFFSET;
	}
	else
	{
		window_rect.left = pim_config->main_window_x;
		window_rect.top  = pim_config->main_window_y;
	}

	if (IsWindowVisible(ui_context->main_window))
	{
		window_rect.top  = ui_context->main_window_pos.y + ui_context->main_window_size.cy + 4;
		window_rect.left = ui_context->main_window_pos.x + pim_config->main_window_anchor_x;

		if (window_rect.top + ui_context->hint_window_size.cy >	monitor_rect.bottom)		//高度越过屏幕边界
			window_rect.top = ui_context->main_window_pos.y - 4 - ui_context->hint_window_size.cy + pim_config->main_window_anchor_y;
	}
	else
		window_rect.top += 4;

	window_rect.right  = window_rect.left + ui_context->hint_window_size.cx;
	window_rect.bottom = window_rect.top + ui_context->hint_window_size.cy;

	MakeRectInRect(&window_rect, monitor_rect);

	SetWindowPos(ui_context->hint_window,
				 0,
				 window_rect.left,
				 window_rect.top,
				 ui_context->hint_window_size.cx,
				 ui_context->hint_window_size.cy,
				 IME_WINDOW_FLAG);
}

void ProcessMouseSelect(PIMCONTEXT *context, int index)
{
	context->selected_digital = 0;
	context->candidate_selected_index = index;

	INPUT inputs[2];
	memset(inputs, 0, sizeof(inputs));

	inputs[0].type		 = INPUT_KEYBOARD;
	inputs[0].ki.wVk	 = 0x20;
	inputs[0].ki.wScan	 = 0x39;
	inputs[0].ki.dwFlags = 0;

	inputs[1].type		 = INPUT_KEYBOARD;
	inputs[1].ki.wVk	 = 0x20;
	inputs[1].ki.wScan	 = 0x39;
	inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

	SendInput(2, inputs, sizeof(INPUT));
}

void ProcessMouseNextPage(PIMCONTEXT *context)
{
	if (!context || !context->ui_context)
		return;

	NextCandidatePage(context);
	PostMessage(context->ui_context->ui_window, UIM_NOTIFY, IMN_CHANGECANDIDATE, 0);
}

void ProcessMousePrevPage(PIMCONTEXT *context)
{
	if (!context || !context->ui_context)
		return;

	PrevCandidatePage(context);
	PostMessage(context->ui_context->ui_window, UIM_NOTIFY, IMN_CHANGECANDIDATE, 0);
}

void EncodeSearchURL(TCHAR* search_url, TCHAR* key, int is_baidu)
{
	if (is_baidu)
	{
		int i, j = 0;
		TCHAR hex_key[0x200] = {0};
		char asc_key[0x200]  = {0};
		static char d2h[]    = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

		Utf16ToAnsi(key, asc_key, sizeof(asc_key));

		for (i = 0; i < (int)strlen(asc_key); i++)
		{
			hex_key[j++] = '%';
			hex_key[j++] = d2h[(byte)asc_key[i] / 0x10];
			hex_key[j++] = d2h[(byte)asc_key[i] % 0x10];
		}

		_tcscpy_s(search_url, MAX_SEARCH_URL_LENGTH, search_prefix_baidu);
		_tcscat_s(search_url, MAX_SEARCH_URL_LENGTH, hex_key);
	}
	else
	{
		_stprintf_s(search_url, MAX_SEARCH_URL_LENGTH, search_prefix_iciba, key);
	}

	return;
}

void ProcessMousePopupMenu(PIMCONTEXT *context, int index)
{
	int i, n, cand_index;
	HMENU hMenuPopup;
	POINT pos;
	TCHAR search_url[MAX_SEARCH_URL_LENGTH] = {0};
	TCHAR item_caption[MAX_CAND_POPUP_MENU_LENGTH] = {0};

	hMenuPopup = CreatePopupMenu();
	if (NULL == hMenuPopup)
		return;

	cand_index = context->candidate_index + index;

	for (i = 0; i < sizeof(candidate_menu_item) / sizeof(candidate_menu_item[0]); i++)
	{
		switch (candidate_menu_item[i].item_type)
		{
		case CAND_POPUP_MENU_SET_TOP:
			switch (context->candidate_array[cand_index].type)
			{
			case CAND_TYPE_ZI:
				if (!(pim_config->hz_option & HZ_USE_FIX_TOP) || (pim_config->hz_output_mode & HZ_OUTPUT_TRADITIONAL))
					continue;
				break;

			case CAND_TYPE_CI:
				if ((pim_config->ci_option & CI_ADJUST_FREQ_NONE) || (pim_config->hz_output_mode & HZ_OUTPUT_TRADITIONAL))
					continue;
				break;

			default:
				continue;
			}
			break;

		case CAND_POPUP_MENU_DELETE:
			if (context->candidate_array[cand_index].type != CAND_TYPE_CI)
				continue;
			break;
		}

		_tcscpy_s(item_caption, MAX_CAND_POPUP_MENU_LENGTH, candidate_menu_item[i].item_caption);
		_tcscat_s(item_caption, MAX_CAND_POPUP_MENU_LENGTH, context->candidate_string[index]);

		AppendMenu(hMenuPopup, MF_STRING, candidate_menu_item[i].item_type, item_caption);
	}

	pos.x = candidate_rect[index].left;
	pos.y = candidate_rect[index].bottom;
	if (!ClientToScreen(context->ui_context->main_window, &pos))
		return;

	n = TrackPopupMenu(hMenuPopup,
					   TPM_LEFTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD,
					   pos.x, pos.y,
					   0, context->ui_context->ui_window, NULL);

	DestroyMenu(hMenuPopup);

	switch (n)
	{
	case CAND_POPUP_MENU_BAIDU:
		EncodeSearchURL(search_url, context->candidate_string[index], 1);
		ExecuateProgram(search_url, 0, 1);
		break;

	case CAND_POPUP_MENU_SET_TOP:
		switch (context->candidate_array[cand_index].type)
		{
		case CAND_TYPE_ZI:
			SetFixTopZi(context, index);
			break;

		case CAND_TYPE_CI:
			int nlen = (int)_tcslen(context->candidate_string[index]);

			InsertCiToCache((HZ*)context->candidate_string[index], nlen, nlen, 1);
			context->modify_flag |= MODIFY_COMPOSE;
			MakeCandidate(context);
			break;
		}

		PostMessage(context->ui_context->ui_window, UIM_NOTIFY, IMN_CHANGECANDIDATE, 1);
		break;

	case CAND_POPUP_MENU_DELETE:
		DeleteCi(context, index);
		PostMessage(context->ui_context->ui_window, UIM_NOTIFY, IMN_CHANGECANDIDATE, 1);
		break;
	}

	return;
}

extern "C" int drag_mode;

LRESULT WINAPI MainWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT paint_struct;
	HDC         dc;
	int			zone, index;			//窗口区域以及索引
	PIMCONTEXT	*context;
	UICONTEXT	*ui_context;
	static int	no_move = 1;

	context = GetPIMContextByWindow(GetUIWindowHandle(window));
	ui_context = GetUIContextByWindow(GetUIWindowHandle(window));

    switch (message)
	{
	case WM_SHOWWINDOW:
		Log(LOG_ID, L"ShowWindow, wParam:%d", wParam);
		return 0;

	case WM_CREATE:
//		SetTimer(window, CARET_TIMER_ID, GetCaretBlinkTime(), 0);
		return 0;

	case WM_DESTROY:
//		KillTimer(window, CARET_TIMER_ID);
		return 0;

    case WM_PAINT:
		dc = BeginPaint(window, &paint_struct);
		if (context)
			PaintMainWindow(context, ui_context, dc);
		EndPaint(window, &paint_struct);
		return 1;

	//窗口移动的时候，需要注意数据以及函数的临界问题。
	case WM_SETCURSOR:
		if (!context)
			return 1;

		if (WM_LBUTTONDOWN == HIWORD(lParam))
			SetCapture(window);

		if (WM_RBUTTONUP == HIWORD(lParam))
		{
			PostMessage(window, WM_RBUTTONUP, 0, 0);
			return 1;
		}

		SetCursor(LoadCursor(0, IDC_ARROW));
		zone = GetMainCursorZone(context, ui_context, &index);
		switch(zone)
		{
		case ZONE_COMPOSE_LEFT:
		case ZONE_COMPOSE_RIGHT:
		case ZONE_NONE:
			if (ui_context->menu_showing)
				break;

//			SetCursor(LoadCursor(0, IDC_SIZEALL));
			if (WM_LBUTTONDOWN == HIWORD(lParam))
				DragStart(window);

			return 1;

		case ZONE_CANDIDATE:
			SetCursor(LoadCursor(0, IDC_HAND));
			return 1;
		}

		return 1;

	//IME窗口的风格设置，使这个消息不会发生，只能在光标消息中处理
	case WM_LBUTTONDOWN:
		DragStart(window);
		no_move = 1;
		return 1;

	case WM_LBUTTONUP:
		if (!context)
			return 1;

		ReleaseCapture();

		DragEnd(window);
		zone = GetMainCursorZone(context, ui_context, &index);
		if (zone == ZONE_CANDIDATE)		//在候选上抬起
		{
			ProcessMouseSelect(context, index);
			return 1;
		}

		if (no_move)			//并没有移动
		{
			if (zone == ZONE_COMPOSE_LEFT)
				ProcessMousePrevPage(context);
			else if (zone == ZONE_COMPOSE_RIGHT)
				ProcessMouseNextPage(context);
			//TODO:还需要将光标位置设定到正常的位置
		}

		RefreshMainWindowPosition(ui_context);
		SetHintWindowPos(ui_context);
		no_move = 1;
		return 1;

	case WM_RBUTTONUP:
		if (!context)
			return 1;

		zone = GetMainCursorZone(context, ui_context, &index);
		if (zone == ZONE_CANDIDATE)
		{
			ProcessMousePopupMenu(context, index);
			return 1;
		}

		ProcessMenu(context, ui_context, TrackMenu(window, context, ui_context, false));
		//ToggleShowStatusWindow(context);
		break;

	case WM_MOUSEMOVE:
		if (drag_mode)
			no_move = 0;
		DragMove(window);
		return 1;

	case WM_MENUSELECT:
		theme_index = -1;
		HideThemeWindow(context, ui_context);

		if (LOWORD(wParam) >= ID_SKIN_BASE && LOWORD(wParam) < ID_SKIN_MAX)
		{
			theme_index = LOWORD(wParam) - ID_SKIN_BASE;
			ShowThemeWindow(context, ui_context);
			return 0;
		}

		return 1;

	case WM_TIMER:
		return 1;

    }

	return DefWindowProc(window, message, wParam, lParam);
}

/*	为配置程序绘制演示的主窗口
 *	如果绘制出的
 */
void PaintDemoMainWindow(PIMCONFIG *config, HDC dc, int *width, int *height, int expand, int vertical, int draw_assist_line)
{
	PIMCONTEXT *context;
	UICONTEXT *ui_context;
	PIMCONFIG *pim_config_sav;
	int cpl_save, cpc_save;				//candidates_per_line, candidate_page_count save

	Lock();

	global_draw_assist_line = draw_assist_line;
	ui_context				= &default_ui_context;
	context					= &demo_context;
	pim_config_sav			= pim_config;
	cpl_save				= pim_config->candidates_per_line;
	cpc_save				= context->candidate_page_count;
	pim_config				= config;

	context->expand_candidate			= expand ? 1 : 0;
	pim_config->show_vertical_candidate = vertical ? 1: 0;

	InitGdiplus();
	LoadThemeResource(ui_context);

	int main_window_min_width, main_window_min_height;
	int use_vertical_bk = use_vertical_background(context, ui_context, config);

	main_window_min_width  = use_vertical_bk ? pim_config->main_window_vert_min_width : pim_config->main_window_min_width;
	main_window_min_height = use_vertical_bk ? pim_config->main_window_vert_min_height : pim_config->main_window_min_height;

	if (theme_previewing)
		_tcscpy_s(hint_message, _SizeOf(hint_message), TEXT(""));
	else
		_tcscpy_s(hint_message, _SizeOf(hint_message), TEXT("提示信息"));

	if (pim_config->input_style == STYLE_ABC)
		PreShowMainWindowABC(context, ui_context, dc);
	else
		PreShowMainWindowCStar(context, ui_context, dc);

	*width  = ui_context->main_window_size.cx;
	*height = ui_context->main_window_size.cy;

	HWND hwnd=WindowFromDC(dc);
	RECT rect;
	GetWindowRect(hwnd,&rect);
	SetWindowPos(hwnd,
				 0,
				 rect.left,
				 rect.top,
				 ui_context->main_window_size.cx,
				 ui_context->main_window_size.cy,
				 IME_WINDOW_FLAG);

	theme_demo_previewing = 1;
	PaintMainWindow(context, ui_context, dc);
	theme_demo_previewing = 0;

	DeleteThemeResource(ui_context);
	FreeGdiplus();

	if (!theme_previewing)
		_tcscpy_s(hint_message, _SizeOf(hint_message), TEXT(""));

	pim_config = pim_config_sav;
	pim_config->candidates_per_line = cpl_save;
	context->candidate_page_count = cpc_save;
	global_draw_assist_line = 0;

	Unlock();
}

LRESULT WINAPI StatusWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	int			zone, index;
	int         last_down_index, last_move_index;
	HDC			dc;
	PAINTSTRUCT	paint_struct;
	PIMCONTEXT	*context;
	UICONTEXT	*ui_context;

	static TRACKMOUSEEVENT track_mouse_event =
	{
		sizeof(TRACKMOUSEEVENT),
		TME_LEAVE | TME_NONCLIENT,
		0,
		HOVER_DEFAULT
	};

	context = GetPIMContextByWindow(GetUIWindowHandle(window));
	ui_context = GetUIContextByWindow(GetUIWindowHandle(window));

    switch (message)
	{
	case WM_NOTIFY:
		return 0;

	case WM_SHOWWINDOW:
		return 0;

	case WM_CREATE:
		//SetTimer(window, 1, 1000, 0);
		return 0;

	case WM_DESTROY:
		//KillTimer(window, 1);
		return 0;

    case WM_PAINT:
		if (!context)
			return 1;

		dc = BeginPaint(window, &paint_struct);
		PaintStatusWindow(context, ui_context, dc);
		EndPaint(window, &paint_struct);
		return 1;

	//窗口移动的时候，需要注意数据以及函数的临界问题。
	case WM_SETCURSOR:
		if (!context)
			return 1;

		//设定鼠标跟踪消息，用于跟踪移出状态窗口判断。
		track_mouse_event.hwndTrack = window;
		_TrackMouseEvent(&track_mouse_event);

		//将鼠标消息发送到ToolTip窗口中。
		PassToToolTipWindow(ui_context, window, HIWORD(lParam));

		if (WM_RBUTTONUP == HIWORD(lParam))
		{
			PostMessage(window, WM_RBUTTONUP, 0, 0);
			return 1;
		}

		if (WM_LBUTTONDOWN == HIWORD(lParam))
			SetCapture(window);

		last_down_index = current_down_button_index;
		last_move_index = current_move_button_index;
		zone = GetStatusCursorZone(ui_context, &index);
		switch(zone)
		{
		case ZONE_DRAG:
			if (ui_context->menu_showing)
				break;

			SetCursor(LoadCursor(0, IDC_SIZEALL));
			if (WM_LBUTTONDOWN == HIWORD(lParam))
				DragStart(window);

			break;

		case ZONE_BUTTON:
			Log(LOG_ID, L"ZONE_BUTTON, menu_showing:%d", ui_context->menu_showing);
			SetCursor(LoadCursor(0, IDC_ARROW));

			if (WM_LBUTTONDOWN == HIWORD(lParam))
			{
				current_down_button_index = index;
				current_move_button_index = -1;
			}
			else if (WM_MOUSEMOVE == HIWORD(lParam))
			{
				current_move_button_index = index;
				current_down_button_index = -1;
			}

			Log(LOG_ID, L"last_move_index:%d, move_index:%d", last_move_index, current_move_button_index);
			if (last_down_index != current_down_button_index ||
				last_move_index != current_move_button_index)
			{
				Log(LOG_ID, L"repaint status");
				dc = GetDC(window);
				PaintStatusWindow(context, ui_context, dc);
				ReleaseDC(window, dc);
			}
			break;

		default:
			SetCursor(LoadCursor(0, IDC_ARROW));
			break;

		}
		return 1;

	case WM_NCMOUSELEAVE:
		if (!context)
			return 1;

		zone = GetStatusCursorZone(ui_context, &index);
		if (zone != ZONE_BUTTON)
			current_down_button_index = current_move_button_index = -1;

		dc = GetDC(window);
		PaintStatusWindow(context, ui_context, dc);
		ReleaseDC(window, dc);

		return 1;

	//IME窗口的风格设置，使这个消息不会发生，只能在光标消息中处理
	case WM_LBUTTONDOWN:
		if (!context)
			return 1;

		DragStart(window);
		return 1;

	case WM_LBUTTONUP:
		if (!context)
			return 1;

		zone = GetStatusCursorZone(ui_context, &index);

		ReleaseCapture();
		DragEnd(window);
		RefreshStatusWindowPosition(ui_context);
		current_move_button_index = current_down_button_index = -1;

		dc = GetDC(window);
		PaintStatusWindow(context, ui_context, dc);
		ReleaseDC(window, dc);

		if (zone == ZONE_BUTTON)
			ClickedButton(context, ui_context, index);

		return 1;

	case WM_RBUTTONUP:
		if (!context)
			return 1;
		ReleaseCapture();
		current_down_button_index = current_move_button_index = -1;
		//dc = GetDC(status_window);
		//PaintStatusWindow(dc);
		//ReleaseDC(status_window, dc);
		ProcessMenu(context, ui_context, TrackMenu(window, context, ui_context, true));
		return 1;

	case WM_MOUSEMOVE:
		if (!context)
			return 1;

		DragMove(window);
		return 1;

	case WM_MENUSELECT:
		theme_index = -1;
		HideThemeWindow(context, ui_context);

		if (LOWORD(wParam) >= ID_SKIN_BASE && LOWORD(wParam) < ID_SKIN_MAX)
		{
			theme_index = LOWORD(wParam) - ID_SKIN_BASE;
			//显示菜单中的主题预览
			ShowThemeWindow(context, ui_context);
			return 0;
		}

		return 1;
    }

	return DefWindowProc(window, message, wParam, lParam);
}

/*	创建主窗口。
 *	参数：
 *		instance		IME实例
 */
void PIM_CreateMainWindow(PIMCONTEXT *context, UICONTEXT *ui_context, HINSTANCE instance)
{
	WNDCLASSEX	main_class;

	//必须先将这个Class删除才行
	UnregisterClass(MAIN_WINDOW_CLASS_NAME, instance);

	//注册主窗口类
	main_class.cbSize		 = sizeof(main_class);
	main_class.style		 = IME_WINDOW_STYLE;
    main_class.cbClsExtra	 = 0;
    main_class.cbWndExtra	 = 0;
    main_class.hIcon		 = 0;
    main_class.hIconSm		 = 0;
	main_class.hInstance	 = instance;
    main_class.hCursor		 = 0;
    main_class.hbrBackground = 0;
    main_class.lpszMenuName  = 0;
	main_class.lpfnWndProc	 = MainWindowProcedure;
	main_class.lpszClassName = MAIN_WINDOW_CLASS_NAME;

	if (!RegisterClassEx(&main_class))
		Log(LOG_ID, L"注册主窗口类失败, err=%d", GetLastError());

	int style = WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;

	//创建主窗口
	ui_context->main_window = CreateWindowEx(
		style,							//ExStyle
		MAIN_WINDOW_CLASS_NAME,			//Class Name
		0,								//Window Name
		WS_POPUP | WS_DISABLED,			//Style
		100,							//Window position
		100,							//Window position
		400,							//Window size
		400,							//Window size
		0,//ui_context->ui_window,		//Parent window
		0,								//Menu
		instance,						//Application instance
		0);								//Parameter

	if (!ui_context->main_window)
	{
		Log(LOG_ID, L"创建主窗口失败, err=%d", GetLastError());
		return;
	}

	SetWindowLongPtr(ui_context->main_window, GWLP_USERDATA, (__int3264)(LONG_PTR)ui_context->ui_window);

	RegisterMainWindow(ui_context->status_window, ui_context->main_window);
}

/*	创建状态窗口
 *	参数：
 *		instance		IME实例
 */
void PIM_CreateStatusWindow(PIMCONTEXT *context, UICONTEXT *ui_context, HINSTANCE instance)
{
	WNDCLASSEX	status_class;

	//先将这个Class删除才行
	UnregisterClass(STATUS_WINDOW_CLASS_NAME, instance);

	//注册主窗口类
	status_class.cbSize			= sizeof(status_class);
	status_class.style			= IME_WINDOW_STYLE;
    status_class.cbClsExtra		= 0;
    status_class.cbWndExtra		= 0;
    status_class.hIcon			= 0;
    status_class.hIconSm		= 0;
	status_class.hInstance		= instance;
    status_class.hCursor		= 0;
    status_class.hbrBackground	= 0;
    status_class.lpszMenuName	= 0;
	status_class.lpfnWndProc	= StatusWindowProcedure;
	status_class.lpszClassName	= STATUS_WINDOW_CLASS_NAME;

	if (!RegisterClassEx(&status_class))
		Log(LOG_ID, L"Register status class failed, err=%d", GetLastError());

	int style = WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;

	//创建主窗口
	ui_context->status_window = CreateWindowEx(
		style,							//Window ExStyle
		STATUS_WINDOW_CLASS_NAME,		//Class Name
		0,								//Window Name
		WS_POPUP | WS_DISABLED,			//Style
		100,							//Window position
		100,							//Window position
		256,							//Window size
		64,								//Window size
		0, //ui_context->ui_window,		//Parent window
//		ui_context->ui_window,			//Parent window
		0,								//Menu
		instance,						//Application instance
		0);								//Parameter

	if (!ui_context->status_window)
		return;

	SetWindowLongPtr(ui_context->status_window, GWLP_USERDATA, (__int3264)(LONG_PTR)ui_context->ui_window);

	InitCommonControls();

	status_buttons = -1;

	//计算每一个Button的位置
	PreShowStatusWindow(context, ui_context);

	current_down_button_index = -1;
	current_move_button_index = -1;

	RegisterStatusWindow(ui_context->status_window);

	//return;
}

/*	显示主窗口
 */
void UpdateMainWindow(PIMCONTEXT *context, UICONTEXT *ui_context)
{
	HDC dc;

	if (!ui_context->main_window)
		return;

	if (!context || (!context->show_composition && !context->show_candidates))
		return;

	Log(LOG_ID, L"更新主窗口");

	//如果出现光标跟踪，一般在width等于0的情况下，所以不特殊处理
	SetMainWindowPosition(ui_context);

	dc = GetDC(ui_context->main_window);
	PaintMainWindow(context, ui_context, dc);
	ReleaseDC(ui_context->main_window, dc);
}

/**	检查当前窗口是否有输入焦点，如果有则采用这个焦点进行位置的设定
 */
void CheckMainWindowPosition(PIMCONTEXT *context, UICONTEXT *ui_context, HWND win)
{
	POINT pt;

	if (ui_context)
		Log(LOG_ID, L"have_caret = %d", ui_context->have_caret);

	if (!context || !ui_context || !pim_config->trace_caret || ui_context->have_caret == 1)
		return;

	if (!GetCaretPos(&pt))		//获取光标不成功则退出
		return;

	if (!pt.x && !pt.y)			//如果为左上角，则为没有光标
		return;

	ClientToScreen(win, &pt);

	if (!pt.x && !pt.y)
		return;

	Log(LOG_ID, L"GetCaretPos: hwnd=%x, pt=<%d, %d>", win, pt.x, pt.y);
	MainWindowTraceCaret(context, ui_context, pt);
}

/**	显示主窗口
 */
void ShowMainWindow(PIMCONTEXT *context, UICONTEXT *ui_context)
{
	int st = clock();
	Log(LOG_ID, L"显示主窗口, owner_window=%x, origin_owner_window:%x");

	if (!context || !ui_context)
	{
		Log(LOG_ID, L"context = 0x%x, ui_context = 0x%x", context, ui_context);
		return;
	}

	if (no_main_show)
		return;

	if (!context->input_length && context->state != STATE_VINPUT)
	{
		Log(LOG_ID, L"没有上下文，不显示主窗口");
		if (IsWindowVisible(ui_context->main_window))
		{
			Log(LOG_ID, L"原来显示主窗口，现在关闭");
			HideMainWindow(context, ui_context);
		}
		return;
	}

	HDC dc = GetDC(ui_context->main_window);
	if (pim_config->input_style == STYLE_CSTAR || context->english_state == ENGLISH_STATE_INPUT)
	{
		//context->force_vertical = 0;
		PreShowMainWindowCStar(context, ui_context, dc);

		RECT monitor_rect = GetMonitorRectFromPoint(ui_context->caret_pos);
		if (monitor_rect.right - monitor_rect.left < ui_context->main_window_size.cx && !context->expand_candidate)
		{
			context->force_vertical		= 1;
			context->expand_candidate	= 0;

			SetCandidatesViewMode(context);

			PreShowMainWindowCStar(context, ui_context, dc);
		}
	}
	else
		PreShowMainWindowABC(context, ui_context, dc);

	ReleaseDC(ui_context->main_window, dc);

	SetMainWindowPosition(ui_context);

	//UpdateMainWindow(context, ui_context);

	if (!IsWindowVisible(ui_context->main_window))
		ShowWindow(ui_context->main_window, SW_SHOWNOACTIVATE);

	UpdateMainWindow(context, ui_context);

	SetHintWindowPos(ui_context);
	CheckHintState(context, ui_context);

	Log(LOG_ID, L"Cost:%d", clock() - st);
}

/*	隐藏主窗口
 */
void HideMainWindow(PIMCONTEXT *context, UICONTEXT *ui_context)
{
	Log(LOG_ID, L"隐藏主窗口");

	if (!context || !ui_context)
		return;

	if (IsWindowVisible(ui_context->main_window))
		ShowWindow(ui_context->main_window, SW_HIDE);

	SetHintWindowPos(ui_context);
	CheckHintState(context, ui_context);
}

/*	处理光标跟随。
 */
void MainWindowTraceCaret(PIMCONTEXT *context, UICONTEXT *ui_context, POINT point)
{
	if (!context || !ui_context)
		return;

	ui_context->caret_pos = point;
	if (IsWindowVisible(ui_context->main_window))
		SetMainWindowPosition(ui_context);

	SetHintWindowPos(ui_context);
	Log(LOG_ID, L"point:<%d,%d>", ui_context->main_window_pos.x, ui_context->main_window_pos.y);
}

/**	更新Hint窗口内容
 */
void UpdateHintWindow(PIMCONTEXT *context, UICONTEXT *ui_context)
{
	static TCHAR hint_string_save[MAX_HINT_LENGTH] = {0};

	if (!_tcscmp(hint_string, hint_string_save))
		return;

	//保存本次的Hint信息
	_tcscpy_s(hint_string_save, _SizeOf(hint_string_save), hint_string);

	UpdateWindow(ui_context->hint_window);
}

/**	隐藏Hint窗口
 */
void HideHintWindow(PIMCONTEXT *context, UICONTEXT *ui_context)
{
	ShowWindow(ui_context->hint_window, SW_HIDE);
}

/**	计算绘制提示窗口大小与位置
 */
void PreShowHintWindow(PIMCONTEXT *context, UICONTEXT *ui_context)
{
	//HFONT old_font;
	//Font *aFont = NULL;
	TCHAR line[MAX_HINT_LENGTH];
	TCHAR *p, *pos;
	int  width, height;
	int  w, h;
	//HDC  dc;
	TESTCONTEXTS;
	//dc = GetDC(ui_context->hint_window);
	Graphics g(ui_context->hint_window);
	width = 0;
	height = HINT_LINE_GAP;
	//old_font = (HFONT)SelectObject(dc, ui_context->hint_font);
	//aFont=ui_context->hint_font;
	_tcscpy_s(line, _SizeOf(line), hint_string);
	p = _tcstok_s(line, TEXT("\n"), &pos);
	while(p)
	{
		//GetTextSize(dc, p, &w, &h);
		GetStringWidth(context, ui_context, p, (int)_tcslen(p), g, &w, &h, ui_context->hint_font, 0);
		p = _tcstok_s(0, TEXT("\n"), &pos);
		height += HINT_LINE_GAP + h;
		if (w > width)
			width = w;
	}
	height += HINT_LINE_GAP;
	width += 2 * HINT_TEXT_GAP;
	ui_context->hint_window_size.cx = width;
	ui_context->hint_window_size.cy = height;
	//SelectObject(dc, old_font);
	//hFont=old_font;
	//ReleaseDC(ui_context->hint_window, dc);
}

/**	显示Hint窗口
 */
void ShowHintWindow(PIMCONTEXT *context, UICONTEXT *ui_context)
{
	if (!context || !ui_context)
		return;

	if (!ui_context->hint_window)
		CreateHintWindow(ui_context);

	PreShowHintWindow(context, ui_context);
	SetHintWindowPos(ui_context);
	ShowWindow(ui_context->hint_window, SW_SHOWNOACTIVATE);
}

/**	绘制Theme main窗口
 */
void PaintThemeMainWindow(PIMCONTEXT *context, UICONTEXT *ui_context, HDC dc, int *width, int *height, PIMCONFIG *config)
{
	if (!ui_context)
		return;

	theme_previewing = 1;
	PaintDemoMainWindow(config, dc, width, height, 0, 0, 0);
	theme_previewing = 0;
}

static int status_button_width_sav;
static int status_button_height_sav;
static int status_window_width_sav;
static int status_window_height_sav;
static int status_buttons_sav;
static int status_button_count_sav;
static int status_button_id_sav[MAX_STATUS_BUTTON_NUMBER];
static POINT status_button_pos_sav[MAX_STATUS_BUTTON_NUMBER];

/**	保存状态窗口的数据，避免设置程序出现错误
 */
void SaveStatusData()
{
	status_button_width_sav		= status_button_width,
	status_button_height_sav	= status_button_height;
	status_window_width_sav		= status_window_width,
	status_window_height_sav	= status_window_height;
	status_buttons_sav			= status_buttons;
	status_button_count_sav		= status_button_count;

	memcpy(status_button_id_sav, status_button_id, sizeof(status_button_id));
	memcpy(status_button_pos_sav, status_button_pos, sizeof(status_button_pos));
}

/**	恢复状态窗口的数据
 */
void RestoreStatusData()
{
	status_button_width		= status_button_width_sav,
	status_button_height	= status_button_height_sav;
	status_window_width		= status_window_width_sav,
	status_window_height	= status_window_height_sav;
	status_buttons			= status_buttons_sav;
	status_button_count		= status_button_count_sav;

	memcpy(status_button_id, status_button_id_sav, sizeof(status_button_id));
	memcpy(status_button_pos, status_button_pos_sav, sizeof(status_button_pos));
}

/*	菜单预览：演示的状态窗口
 */
void PaintDemoStatusWindow(PIMCONFIG *config, HDC dc, int *width, int *height, int draw_assist_line)
{
	PIMCONFIG *pim_config_sav;
	PIMCONTEXT *context;
	UICONTEXT *ui_context;

	Lock();

	SaveStatusData();

	global_draw_assist_line = draw_assist_line;
	context = &demo_context;
	ui_context = &default_ui_context;

	pim_config_sav = pim_config;
	pim_config = config;
	status_buttons = -1;

	InitGdiplus();
	LoadThemeResource(ui_context);

	PreShowStatusWindow(context, ui_context);
	*width = status_window_width;
	*height = status_window_height;

	PaintStatusWindow(context, ui_context, dc);

	DeleteThemeResource(ui_context);
	FreeGdiplus();

	pim_config = pim_config_sav;
	global_draw_assist_line = 0;

	RestoreStatusData();

	Unlock();
}

/**	菜单预览：绘制Theme Status窗口
 */
void PaintThemeStatusWindow(PIMCONTEXT *context, UICONTEXT *ui_context, HDC dc, int *width, int *height, PIMCONFIG *config)
{
	if (!ui_context)
		return;

	PaintDemoStatusWindow(config, dc, width, height, 0);
}

LRESULT WINAPI ThemeMainWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	HDC			dc;
	PAINTSTRUCT	paint_struct;
	PIMCONTEXT	*context;
	UICONTEXT	*ui_context;
	int			width = 400, height = 400;

	context = GetPIMContextByWindow(GetUIWindowHandle(window));
	ui_context = GetUIContextByWindow(GetUIWindowHandle(window));

	switch (message)
	{
	case WM_SHOWWINDOW:
		return 0;

	case WM_CREATE:
		return 0;

	case WM_DESTROY:
		return 0;

	case WM_PAINT:
		dc = BeginPaint(window, &paint_struct);
		PaintThemeMainWindow(context, ui_context, dc, &width, &height, &pim_config_theme);
		EndPaint(window, &paint_struct);

		return 1;
	}

	return DefWindowProc(window, message, wParam, lParam);
}

LRESULT WINAPI ThemeStatusWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	HDC			dc;
	PAINTSTRUCT	paint_struct;
	PIMCONTEXT	*context;
	UICONTEXT	*ui_context;
	int			width = 256, height = 64;

	context = GetPIMContextByWindow(GetUIWindowHandle(window));
	ui_context = GetUIContextByWindow(GetUIWindowHandle(window));

	switch (message)
	{
	case WM_SHOWWINDOW:
		return 0;

	case WM_CREATE:
		return 0;

	case WM_DESTROY:
		return 0;

	case WM_PAINT:
		dc = BeginPaint(window, &paint_struct);
		PaintThemeStatusWindow(context, ui_context, dc, &width, &height, &pim_config_theme);
		EndPaint(window, &paint_struct);

		return 1;
	}

	return DefWindowProc(window, message, wParam, lParam);
}

/*	创建Theme预览窗口
 */
int CreateThemeWindow(UICONTEXT *ui_context)
{
	WNDCLASSEX	theme_main_class, theme_status_class;
	int style = WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW;

	if (!ui_context->theme_main_window)
	{
		UnregisterClass(THEME_MAIN_WINDOW_CLASS_NAME, global_instance);

		//注册主窗口类
		theme_main_class.cbSize			= sizeof(theme_main_class);
		theme_main_class.style			= IME_WINDOW_STYLE;
		theme_main_class.cbClsExtra		= 0;
		theme_main_class.cbWndExtra		= 0;
		theme_main_class.hIcon			= 0;
		theme_main_class.hIconSm		= 0;
		theme_main_class.hInstance		= global_instance;
		theme_main_class.hCursor		= 0;
		theme_main_class.hbrBackground	= 0;
		theme_main_class.lpszMenuName	= 0;
		theme_main_class.lpfnWndProc	= ThemeMainWindowProcedure;
		theme_main_class.lpszClassName	= THEME_MAIN_WINDOW_CLASS_NAME;

		RegisterClassEx(&theme_main_class);

		//创建theme预览窗口
		ui_context->theme_main_window = CreateWindowEx(
			style,
			THEME_MAIN_WINDOW_CLASS_NAME,	//Class Name
			0,								//Window Name
			WS_POPUP | WS_DISABLED,			//Style
			0,								//Window position
			0,								//Window position
			400,							//Window size
			400,							//Window size
			0,								//Parent window
			0,								//Menu
			global_instance,				//Application instance
			0);								//Parameter

		if (ui_context->theme_main_window)
			SetWindowLongPtr(ui_context->theme_main_window, GWLP_USERDATA, (__int3264)(LONG_PTR)ui_context->ui_window);
	}

	if (!ui_context->theme_status_window)
	{
		UnregisterClass(THEME_STATUS_WINDOW_CLASS_NAME, global_instance);

		//注册主窗口类
		theme_status_class.cbSize		 = sizeof(theme_status_class);
		theme_status_class.style		 = IME_WINDOW_STYLE;
		theme_status_class.cbClsExtra	 = 0;
		theme_status_class.cbWndExtra	 = 0;
		theme_status_class.hIcon		 = 0;
		theme_status_class.hIconSm		 = 0;
		theme_status_class.hInstance	 = global_instance;
		theme_status_class.hCursor		 = 0;
		theme_status_class.hbrBackground = 0;
		theme_status_class.lpszMenuName  = 0;
		theme_status_class.lpfnWndProc	 = ThemeStatusWindowProcedure;
		theme_status_class.lpszClassName = THEME_STATUS_WINDOW_CLASS_NAME;

		RegisterClassEx(&theme_status_class);

		//创建theme预览窗口
		ui_context->theme_status_window = CreateWindowEx(
			style,
			THEME_STATUS_WINDOW_CLASS_NAME,	//Class Name
			0,								//Window Name
			WS_POPUP | WS_DISABLED,			//Style
			0,								//Window position
			0,								//Window position
			256,							//Window size
			64,								//Window size
			0,								//Parent window
			0,								//Menu
			global_instance,				//Application instance
			0);								//Parameter

		if (ui_context->theme_status_window)
			SetWindowLongPtr(ui_context->theme_status_window, GWLP_USERDATA, (__int3264)(LONG_PTR)ui_context->ui_window);
	}

	return 1;
}

/**	菜单中的主题预览：计算绘制Theme窗口大小与位置
 */
void PreShowThemeWindow(PIMCONTEXT *context, UICONTEXT *ui_context)
{
	HDC			dc;
	int			left, top;
	int			main_width = 400, main_height = 400;
	int			status_width = 256, status_height = 64;
	POINT		pos;
	HWND		hwnd;
	WINDOWINFO	wininfo;

	if (theme_index < 0 || theme_index > MAX_THEME_COUNT)
		return;

	memcpy(&pim_config_theme, pim_config, sizeof(PIMCONFIG));
	_tcscpy_s(pim_config_theme.theme_name, MAX_THEME_NAME_LENGTH, theme_list[theme_index]);

	LoadThemeConfig(&pim_config_theme);

	pim_config_theme.input_style = STYLE_CSTAR;
	pim_config_theme.select_sytle = SELECTOR_DIGITAL;

	dc = GetDC(ui_context->theme_main_window);
	PaintThemeMainWindow(context, ui_context, dc, &main_width, &main_height, &pim_config_theme);
	ReleaseDC(ui_context->theme_main_window, dc);

	dc = GetDC(ui_context->theme_status_window);
	PaintThemeStatusWindow(context, ui_context, dc, &status_width, &status_height, &pim_config_theme);
	ReleaseDC(ui_context->theme_status_window, dc);

	GetCursorPos(&pos);
	hwnd = WindowFromPoint(pos);

	wininfo.cbSize = sizeof(wininfo);
	GetWindowInfo(hwnd, &wininfo);

	left = wininfo.rcWindow.right + 5;
	top  = wininfo.rcWindow.top + 5;

	RECT monitor_rect = GetMonitorRectFromPoint(pos);

	if (left + main_width > monitor_rect.right)
		left = wininfo.rcWindow.left - main_width - 5;

	SetWindowPos(ui_context->theme_main_window, 0, left, top, main_width, main_height, IME_WINDOW_FLAG);
	SetWindowPos(ui_context->theme_status_window, 0, left, top + main_height + 10, status_width, status_height, IME_WINDOW_FLAG);
}

/**	菜单中的主题预览：显示Theme窗口
 */
void ShowThemeWindow(PIMCONTEXT *context, UICONTEXT *ui_context)
{
	if (!context || !ui_context)
		return;

	if (theme_index < 0 || theme_index > MAX_THEME_COUNT)
		return;

	if (!ui_context->theme_main_window || !ui_context->theme_status_window)
		CreateThemeWindow(ui_context);

	PreShowThemeWindow(context, ui_context);
	ShowWindow(ui_context->theme_main_window, SW_SHOWNOACTIVATE);
	ShowWindow(ui_context->theme_status_window, SW_SHOWNOACTIVATE);
}

/**	隐藏Theme窗口
 */
void HideThemeWindow(PIMCONTEXT *context, UICONTEXT *ui_context)
{
	if (ui_context->theme_main_window)
	{
		DestroyWindow(ui_context->theme_main_window);
		ui_context->theme_main_window = 0;
	}

	if (ui_context->theme_status_window)
	{
		DestroyWindow(ui_context->theme_status_window);
		ui_context->theme_status_window = 0;
	}
}

void CheckHintState(PIMCONTEXT *context, UICONTEXT *ui_context)
{
	if (!context || !ui_context)
		return;

	if (!IsWindowVisible(ui_context->status_window) || !pim_config->show_sp_hint ||
		pim_config->pinyin_mode != PINYIN_SHUANGPIN || context->english_state != ENGLISH_STATE_NONE ||
		(context->state != STATE_START && context->state != STATE_EDIT && context->state != STATE_ILLEGAL &&
		 !(context->state == STATE_SOFTKBD && context->softkbd_index == SOFTKBD_NUMBER - 1)))
	{
		HideHintWindow(context,ui_context);
		return;
	}

	GetSPHintString(context, hint_string, _SizeOf(hint_string));
	if (!hint_string[0])
	{
		HideHintWindow(context, ui_context);
		return;
	}

	ShowHintWindow(context, ui_context);
}

/*	隐藏状态窗口
 */
void HideStatusWindow(PIMCONTEXT *context, UICONTEXT *ui_context)
{
	Log(LOG_ID, L"隐藏状态窗口");
	if (!context || !ui_context)
		return;

	ShowWindow(ui_context->status_window, SW_HIDE);
	if (context && context->soft_keyboard)
		HideSoftKBDWindow();

	if (pim_config && pim_config->show_sp_hint && pim_config->pinyin_mode == PINYIN_SHUANGPIN)
		HideHintWindow(context, ui_context);
}

/*	更新状态窗口
 */
void UpdateStatusWindow(PIMCONTEXT *context, UICONTEXT *ui_context)
{
	HDC dc;

	if (!context || !ui_context || !context->show_status || !ui_context->status_window)
		return;

	Log(LOG_ID, L"更新状态窗口");

	PreShowStatusWindow(context, ui_context);

	if (ui_context->status_window)
	{
		dc = GetDC(ui_context->status_window);
		PaintStatusWindow(context, ui_context, dc);
		ReleaseDC(ui_context->status_window, dc);
	}
}

/**	显示状态窗口
 */
void ShowStatusWindow(PIMCONTEXT *context, UICONTEXT *ui_context)
{
	if (no_status_show)
		return;

	static int config_show_status = -1;

	Log(LOG_ID, L"显示状态窗口");

	if (config_show_status == -1)		//第一次
		config_show_status = pim_config->show_status_window;
	else if (config_show_status && !pim_config->show_status_window)
		config_show_status = context->show_status = 0;
	else if (!config_show_status && pim_config->show_status_window)
		config_show_status = context->show_status = 1;

	//全屏时隐藏状态栏 and 当前是全屏应用
	if (pim_config->hide_status_when_full_screen && IsFullScreen())
	{
		config_show_status = context->show_status = 0;
		if (IsWindowVisible(ui_context->status_window))
			HideStatusWindow(context, ui_context);
	}

	if (!context || !ui_context || !context->show_status)
		return;

	SetStatusWindowPosition(ui_context);
	UpdateStatusWindow(context, ui_context);
	ShowWindow(ui_context->status_window, SW_SHOWNOACTIVATE);

	if (context)
	{	if (context->soft_keyboard)
			ShowSoftKBDWindow();
		else
			HideSoftKBDWindow();
	}

	if (pim_config->show_sp_hint && pim_config->pinyin_mode == PINYIN_SHUANGPIN)
		CheckHintState(context, ui_context);
}

/*	清除主窗口
 */
void PIM_DestroyMainWindow(PIMCONTEXT *context, UICONTEXT *ui_context, HINSTANCE instance)
{
	if (context && ui_context)
	{
		DestroyWindow(ui_context->main_window);
		UnregisterClass(MAIN_WINDOW_CLASS_NAME, instance);
		ui_context->main_window = 0;
	}

	return;
}

/*	清除状态窗口
 */
void PIM_DestroyStatusWindow(PIMCONTEXT *context, UICONTEXT *ui_context, HINSTANCE instance)
{
	if (context && ui_context)
	{
		DestroyWindow(ui_context->status_window);
		UnregisterClass(STATUS_WINDOW_CLASS_NAME, instance);
		UnregisterStatusWindow(ui_context->status_window);
		ui_context->status_window = 0;
	}

	return;
}

/*	清除提示窗口
 */
void PIM_DestroyHintWindow(PIMCONTEXT *context, UICONTEXT *ui_context, HINSTANCE instance)
{
	if (context && ui_context && ui_context->hint_window)
	{
		DestroyWindow(ui_context->hint_window);
		UnregisterClass(HINT_WINDOW_CLASS_NAME, instance);
		ui_context->hint_window = 0;
	}
}

/*	清除ToolTip窗口
 */
void PIM_DestroyToolTipWindow(PIMCONTEXT *context, UICONTEXT *ui_context, HINSTANCE instance)
{
	if (context && ui_context && ui_context->tooltip_window)
	{
		DestroyWindow(ui_context->tooltip_window);
		ui_context->tooltip_window = 0;
	}
}

/*	清除Theme预览窗口
 */
void PIM_DestroyThemeWindow(PIMCONTEXT *context, UICONTEXT *ui_context, HINSTANCE instance)
{
	if (context && ui_context && ui_context->theme_main_window)
	{
		DestroyWindow(ui_context->theme_main_window);
		UnregisterClass(THEME_MAIN_WINDOW_CLASS_NAME, instance);
		ui_context->theme_main_window = 0;
	}

	if (context && ui_context && ui_context->theme_status_window)
	{
		DestroyWindow(ui_context->theme_status_window);
		UnregisterClass(THEME_STATUS_WINDOW_CLASS_NAME, instance);
		ui_context->theme_status_window = 0;
	}
}

//配置界面/主题制作：绘制主题效果图
void WINAPI PaintDemoWindow(PIMCONFIG *config, HDC dc_status, HDC dc_main,
							int *status_width, int *status_height, int *main_width, int *main_height,
							int expand, int vertical, int draw_assist_line)
{
	PIMCONFIG *pim_config_sav;
	PIMCONTEXT *context;
	UICONTEXT *ui_context;
	int cpl_save, cpc_save;				//candidates_per_line, candidate_page_count save

	Lock();

	SaveStatusData();

	global_draw_assist_line = draw_assist_line;
	context					= &demo_context;
	ui_context				= &default_ui_context;
	pim_config_sav			= pim_config;
	cpl_save				= pim_config->candidates_per_line;
	cpc_save				= context->candidate_page_count;
	pim_config				= config;
	status_buttons			= -1;

	InitGdiplus();
	LoadThemeResource(ui_context);

	PreShowStatusWindow(context, ui_context);
	*status_width = status_window_width;
	*status_height = status_window_height;

	PaintStatusCanvas(context, ui_context, dc_status);

	context->expand_candidate = expand ? 1 : 0;
	pim_config->show_vertical_candidate = vertical ? 1 : 0;

	SetCandidatesViewMode(context);

	int main_window_min_width, main_window_min_height;
	int use_vertical_bk = use_vertical_background(context, ui_context, config);

	main_window_min_width  = use_vertical_bk ? pim_config->main_window_vert_min_width : pim_config->main_window_min_width;
	main_window_min_height = use_vertical_bk ? pim_config->main_window_vert_min_height : pim_config->main_window_min_height;

	if (theme_previewing)
		_tcscpy_s(hint_message, _SizeOf(hint_message), TEXT(""));
	else
		_tcscpy_s(hint_message, _SizeOf(hint_message), TEXT("提示信息"));

	if (pim_config->input_style == STYLE_ABC)
		PreShowMainWindowABC(context, ui_context, dc_main);
	else
		PreShowMainWindowCStar(context, ui_context, dc_main);

	*main_width = ui_context->main_window_size.cx;
	*main_height = ui_context->main_window_size.cy;

	theme_demo_previewing = 1;
	PaintMainCanvas(context, ui_context, dc_main);
	theme_demo_previewing = 0;

	if (!theme_previewing)
		_tcscpy_s(hint_message, _SizeOf(hint_message), TEXT(""));

	DeleteThemeResource(ui_context);
	FreeGdiplus();

	pim_config						= pim_config_sav;
	pim_config->candidates_per_line = cpl_save;
	context->candidate_page_count	= cpc_save;
	global_draw_assist_line			= 0;

	RestoreStatusData();

	Unlock();
}

static GdiplusStartupInput gdiplusStartupInput;
static ULONG_PTR           gdiplusToken;

void InitGdiplus()
{
	//初始化GDI+
	int ret = GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	if (ret != Gdiplus::Ok)
		MessageBox(0, TEXT("GdiPlus初始化失败，可能系统没有安装GdiPlus.Dll。\n")
					  TEXT("安装GdiPlus.dll之后，系统工作将会正常"), TEXT("初始化失败"), MB_OK);
}

void FreeGdiplus()
{
	//为了解决PowerDesigner12的无法shutdown gdi+的问题，
	//只好先注释上. 2007-7-19

	if (no_gdiplus_release)		//不能进行Gdiplus的释放（传奇世界）
		return;

	//释放GDI+
	GdiplusShutdown(gdiplusToken);
}

static Bitmap *PIM_LoadImage(const TCHAR *image_name)
{
	Bitmap *image;

	image = new Bitmap(image_name);
	if (image->GetLastStatus() != Ok)
	{
		delete image;
		return 0;
	}

	return image;
}

Bitmap *LoadImageResource(const TCHAR *image_name)
{
	TCHAR name[MAX_PATH];
	Bitmap *bitmap;

	//先根据绝对路径进行寻找
	bitmap = PIM_LoadImage(image_name);
	if (bitmap)
		return bitmap;

	GetFileFullName(TYPE_ALLAPP, image_name, name);
	bitmap = PIM_LoadImage(name);

	if (!bitmap)
		Log(LOG_ID, L"装载图象失败，image=%s\n", image_name);

	return bitmap;
}

/*	装载主题所需要的资源
 */
void LoadThemeResource(UICONTEXT *ui_context)
{
	Lock();

	PIM_CreateFonts(ui_context);

	ui_context->image_main_bk        = LoadImageResource(pim_config->main_image_name);
	ui_context->image_main_line      = LoadImageResource(pim_config->main_line_image_name);
	ui_context->image_main_vert_bk   = LoadImageResource(pim_config->main_vert_image_name);
	ui_context->image_main_vert_line = LoadImageResource(pim_config->main_vert_line_image_name);
	ui_context->image_status_bk      = LoadImageResource(pim_config->status_image_name);
	ui_context->image_status_buttons = LoadImageResource(pim_config->status_buttons_image_name);

	Unlock();
}

/**	检查主题资源是否还在（为了处理Explorer多线程造成的资源删除）
 */
void CheckThemeResource(UICONTEXT *ui_context)
{
	if (!ui_context->image_main_bk)
		LoadThemeResource(ui_context);
}

void FreeAndNil(Bitmap  *bitmap)
{
	if (bitmap)
	{
		delete bitmap;
		bitmap = 0;
	}
}

/*	卸载主题的资源
 */
void DeleteThemeResource(UICONTEXT *ui_context)
{
	if (!ui_context)
		return;

	PIM_DestroyFonts(ui_context);

	FreeAndNil(ui_context->image_main_bk);
	FreeAndNil(ui_context->image_main_line);
	FreeAndNil(ui_context->image_main_vert_bk);
	FreeAndNil(ui_context->image_main_vert_line);
	FreeAndNil(ui_context->image_status_bk);
	FreeAndNil(ui_context->image_status_buttons);
}

/*	创建IME使用的三个窗口
 */
void CreateIMEWindows(PIMCONTEXT *context, UICONTEXT *ui_context)
{
	InitGdiplus();													//启动GDI+
	LoadThemeResource(ui_context);									//装载资源
	PIM_CreateStatusWindow(context, ui_context, global_instance);	//创建状态窗口
	PIM_CreateMainWindow(context, ui_context, global_instance);		//创建主窗口
}

/*	释放IME使用的三个窗口
 */
void DestroyIMEWindows(PIMCONTEXT *context, UICONTEXT *ui_context)
{
	PIM_DestroyMainWindow(context, ui_context, global_instance);	//清除主窗口
	PIM_DestroyStatusWindow(context, ui_context, global_instance);	//清除状态窗口
	PIM_DestroyHintWindow(context, ui_context, global_instance);	//清除提示窗口
	PIM_DestroyToolTipWindow(context, ui_context, global_instance);	//清除ToolTip窗口
	PIM_DestroyThemeWindow(context, ui_context, global_instance);	//清除Theme预览窗口
	DeleteThemeResource(ui_context);								//清除资源
	FreeGdiplus();													//关闭GDI+
}

/**	设置索引字符串
 */
void SetHintString(const TCHAR *str)
{
	_tcscpy_s(hint_string, _SizeOf(hint_string), str);
}

UICONTEXT *AllocateUIContext()
{
	return (UICONTEXT*)malloc(sizeof(UICONTEXT));
}

void FreeUIContext(UICONTEXT *context)
{
	free(context);
}