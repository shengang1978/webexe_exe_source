#include "BoardService.h"
#include "COSInfo.h"
#include "DataReport.h"
#include "Base.h"
BoardService& BoardService::instance()
{
	static BoardService instance;
	return instance;
}

void BoardService::setCallback(BoardServiceCallback* callback)
{
	_callback = callback;
	sendStatusChanged();
}

HWND BoardService::getRenderWindow() const
{
	return _board->getRenderWindow();
}

void BoardService::appendActionsData(std::string& data) const
{
	_board->appendActionsData(data.c_str(), data.size());
}

void BoardService::appendEventData(std::string& data) const
{
	_board->appendActionsData(data.c_str(), data.size());
}

void BoardService::useTool(BoardTool tool) const
{
	_board->useTool(tool);
}

void BoardService::setWidth(uint32_t width) const
{
	_board->setWidth(width);
}

void BoardService::setColor(uint32_t rgba) const
{
	_board->setColor(rgba);
}

void BoardService::setFill(bool fill) const
{
	_board->setFill(fill);
}

void BoardService::undo() const
{
	_board->undo();
}

void BoardService::redo() const
{
	_board->redo();
}

void BoardService::clear() const
{
	_board->clear();
}

void BoardService::syncEventData()
{
	syncEvent();
}

void BoardService::uploadFile(const std::wstring& fileName)
{
	if (!m_bFirstDownloaded)
	{
		std::string whiteboardReport = DataReport::instance().getWhiteboardReport();
		HttpReportRequest::instance().reportELK(whiteboardReport);
	}
	m_bFirstDownloaded = false;
	fetchCosSig(fileName);
}

uint32_t BoardService::getPageIndex() const
{
	return _pageIndex;
}

uint32_t BoardService::getPageCount() const
{
	return _pagesId.size();
}

void BoardService::gotoPage(uint32_t pageIndex)
{
	if(pageIndex < _pagesId.size())
	{
		_pageIndex = pageIndex;
		for(int i = static_cast<int>(_pageIndex) - 2; i < static_cast<int>(_pageIndex) + 2; ++i)
		{
			if(i >= 0 && i < _pagesId.size() && !_backsSend[i] && !_backsUrl[i].empty())
			{
				_board->useBackground(_backsUrl[i].c_str(), _pagesId[i].c_str());
				//_backsSend[i] = true;
			}
		}
		_board->pageOperate(_pagesId[_pageIndex].c_str());
	}
}

void BoardService::gotoCurrentPage()
{
    if (_pageIndex >= 0)
    {
        gotoPage(_pageIndex);
    }
}

void BoardService::reportELK()
{
	if (!m_bFirstDownloaded)
	{
		m_bFirstDownloaded = true;
		std::string whiteboardReport = DataReport::instance().getWhiteboardReport();
		HttpReportRequest::instance().reportELK(whiteboardReport);
	}
}

void BoardService::gotoLastPage()
{
	if(_pageIndex > 0)
	{
		gotoPage(_pageIndex - 1);
	}
}

void BoardService::gotoNextPage()
{
	if (_pageIndex < _pagesId.size() - 1)
	{
		gotoPage(_pageIndex + 1);
	}
}

void BoardService::insertPage()
{
	uint32_t toPageID = _pageIndex + 1;
	_pagesId.emplace(_pagesId.begin() + toPageID, m_authData.userID + "_" + std::to_string(time(nullptr)) + "_" + std::to_string(_pagesId.size() + 1));
	_backsUrl.emplace(_backsUrl.begin() + toPageID, L"");
	_backsSend.emplace(_backsSend.begin() + toPageID, false);
	gotoPage(toPageID);
}

void BoardService::deletePage()
{
	if(_pagesId.size() > 1)
	{
		std::string deletePage = _pagesId[_pageIndex];
		_pagesId.erase(_pagesId.begin() + _pageIndex);
		_backsUrl.erase(_backsUrl.begin() + _pageIndex);
		_backsSend.erase(_backsSend.begin() + _pageIndex);
		uint32_t toPageIndex = _pageIndex;
		if (toPageIndex >= _pagesId.size())
		{
			--toPageIndex;
		}
		auto deletePages = std::make_unique<const char*[]>(1);
		deletePages.get()[0] = deletePage.c_str();
		_board->pageOperate(_pagesId[toPageIndex].c_str(), deletePages.get(), 1);

		gotoPage(toPageIndex);
	}
}

void BoardService::init(const BoardAuthData & authData)
{
	m_authData = authData;

	_board = new BoardSDK(m_authData.userID.c_str());
	_board->setCallback(this);
}

void BoardService::setRoomID(const std::string & roomID)
{
	m_roomID = roomID;
}

void BoardService::sendUploadProgress(int percent) const
{
	if (_callback) {
		_callback->onUploadProgress(percent);
	}
}

void BoardService::sendUploadResult(bool success) const
{
	if (_callback) {
		_callback->onUploadResult(success);
	}
}

void BoardService::sendStatusChanged() const
{
	if (_callback)
	{
		_callback->onStatusChanged(_canUndo, _canRedo, _canCopy, _canRemove);
	}
}

void BoardService::sendSyncEventResult(bool success) const
{
	if (_callback)
	{
		_callback->onSyncEventResult(success);
	}
}

std::wstring BoardService::url() const
{
	std::wstring url = L"https://sxb.qcloud.com/conf_svr_sdk/conference_server/public/api/conference?sdkappid=";
	url.append(std::to_wstring(m_authData.sdkAppID));
	if (!m_authData.token.empty())
	{
		url.append(L"&identifier=")
			.append(TXCHttpRequest::a2w(m_authData.userID))
			.append(L"&user_token=")
			.append(TXCHttpRequest::a2w(m_authData.token));
	}
	return url;
}

void BoardService::fetchCosSig(const std::wstring& fileName)
{
	HttpHeadersPtr headers = std::make_shared<HttpHeaders>();
	headers->set_content_type(L"application/json; charset=utf-8");

	//获取COS上传权限签名
	_cos.httpClient()->asyn_post(
		url(),
		headers,
		CosSigReq("pc/", "").GenReq(),
		[=](int code, std::string resp)
	{
		CosSigRsp sigRsp;
		sigRsp.Parse(resp);
		if (sigRsp.GetCode())
		{
			DataReport::instance().setFetchCosSigCode(sigRsp.GetCode());
			sendUploadResult(false);
		}
		else
		{
			_cos.setAppID(L"1253488539");
			_cos.setBucket(TXCHttpRequest::a2w(sigRsp.GetBucket()));
			_cos.setPath(L"pc/");
			_cos.setRegion(TXCHttpRequest::a2w(sigRsp.GetRegion()));
			uploadToCos(sigRsp.GetSig(), fileName);
		}
	});
}

void BoardService::uploadToCos(const std::string& sig, const std::wstring& fileName)
{
	_cosSign = sig;
	const std::wstring objName = std::to_wstring(static_cast<uint32_t>(time(nullptr))) + L"_" + TXCCosHelper::getFileName(fileName);
	const std::wstring uploadurl = _cos.getUploadUrl(objName);
	DataReport::instance().setUploadUrl(Wide2UTF8(uploadurl));
	//COS上传
	_cos.uploadObject(
		fileName,
		objName,
		sig,
		[=](int code, bool done)
		{
			if (done)
			{
				DataReport::instance().setUploadtoCosCode(code);
				if (code != 200)
				{
					sendUploadResult(false);
					return;
				}
				previewFile(objName);
			}
			else
			{
				sendUploadProgress(code);
			}
		});
}

void BoardService::previewFile(const std::wstring& objName)
{
	const std::size_t pos = objName.find_last_of(L".");
	const std::wstring ext = objName.substr(pos + 1);
	const bool transCode =
		ext == L"ppt" ||
		ext == L"pptx" ||
		ext == L"pdf" ||
		ext == L"doc" ||
		ext == L"docx";

	if (!transCode) //非转码文件
	{
		//删除旧的页面
		auto oldPages = std::make_unique<const char*[]>(_pagesId.size() - 1);
		for (size_t i = 1; i < _pagesId.size(); ++i)
		{
			oldPages.get()[i - 1] = _pagesId[i].c_str();
		}
		_board->pageOperate("#DEFAULT", oldPages.get(), _pagesId.size() - 1);
		_pagesId.clear();
		_backsUrl.clear();
		_backsSend.clear();
		//设置新的背景图
		_pagesId.emplace_back("#DEFAULT");
		_backsUrl.emplace_back(_cos.getDownloadUrl(objName).c_str());
		_backsSend.emplace_back(false);
		gotoPage(0);
		sendUploadResult(true);
		return;
	}

	std::wstring previewUrl = _cos.getPreviewUrl(objName, 1);
	DataReport::instance().setPreviewUrl(Wide2UTF8(previewUrl));

	//转码文件：分页处理
	_cos.previewObject(
		objName,
		_cosSign,
		1,
		L"",
		[=](int page_count, std::string file_content)
		{
			DataReport::instance().setPageCount(page_count);
			if (page_count > 0)
			{
				//删除旧的页面
				auto oldPages = std::make_unique<const char*[]>(_pagesId.size() - 1);
				for (size_t i = 1; i < _pagesId.size(); ++i)
				{
					oldPages.get()[i-1] = _pagesId[i].c_str();
				}
				_board->pageOperate("#DEFAULT", oldPages.get(), _pagesId.size() - 1);
				_pagesId.clear();
				_backsUrl.clear();
				_backsSend.clear();
				//设置新的背景图
				_pagesId.emplace_back("#DEFAULT");
				_backsUrl.emplace_back(_cos.getPreviewUrl(objName, 1));
				_backsSend.emplace_back(false);
				for (int i = 2; i <= page_count; ++i)
				{
					_pagesId.emplace_back(
						m_authData.userID + "_" + std::to_string(time(nullptr)) + "_" + std::to_string(i));
					_backsUrl.emplace_back(_cos.getPreviewUrl(objName, i));
					_backsSend.emplace_back(false);
				}
				gotoPage(0);
				sendUploadResult(true);
			}
			else
			{
				sendUploadResult(false);
			}
		});
}

void BoardService::reportEvent(Json::Value event)
{
	/*HttpHeadersPtr headers = std::make_shared<HttpHeaders>();
	headers->set_content_type(L"application/json; charset=utf-8");

	_cos.httpClient()->asyn_post(
		url(),
		headers,
		ReportWhiteBoardReq(Context::instance().roomID(), event).GenReq(),
		[=](int code, std::string resp)
		{
			ReportWhiteBoardRsp rwbRsp;
			rwbRsp.Parse(resp);
			if (rwbRsp.GetCode())
			{
			}
		});*/
}

void BoardService::syncEvent()
{
	sendSyncEventResult(false);
	//HttpHeadersPtr headers = std::make_shared<HttpHeaders>();
	//headers->set_content_type(L"application/json; charset=utf-8");

	//_cos.httpClient()->asyn_post(
	//	url(),
	//	headers,
	//	GetWhiteBoardReq(Context::instance().roomID()).GenReq(),
	//	[=](int code, std::string resp)
	//{
	//	GetWhiteBoardRsp gwbRsp;
	//	gwbRsp.Parse(resp);
	//	if ( gwbRsp.GetCode() == 0)
	//	{
	//		Json::FastWriter writer;
	//		const std::string data = writer.write(gwbRsp.GetBoardDataList());
	//		_board->appendBoardEventData(data.c_str(), data.size());
	//		sendSyncEventResult(true);
	//	}
	//	else
	//	{
	//		sendSyncEventResult(false);
	//	}
	//});
}

void BoardService::onActionsData(const char* data, uint32_t length)
{
	if (m_roomID.empty())
		return;

	TIMManager::instance()->sendWhiteBoardData(m_roomID.c_str(), data, length);
}

void BoardService::onStatusChanged(bool canUndo, bool canRedo, bool canCopy, bool canRemove)
{
	_canUndo = canUndo;
	_canRedo = canRedo;
	_canCopy = canCopy;
	_canRemove = canRemove;
	sendStatusChanged();
}

uint32_t BoardService::onGetTime()
{
	return time(nullptr);
}

void BoardService::onRecvWhiteBoardData(const char* data, uint32_t length)
{
	_board->appendActionsData(data, length);
}

void BoardService::onGetBoardData(bool bResult) //拉取上一次数据成功
{
	//initBoardsData();
	//_fid = getCurrentFile();
	//refreshPageInfo();
	//if (!_pageCount)
	//{
	//	addPage("#DEFAULT");
	//	refreshPageInfo();
	//}
	//if (_callback) {
	//	_callback->onGetBoardData(bResult);
	//}
}

void BoardService::onRenderFrame()
{
	if (!m_bFirstDownloaded)
	{
		m_bFirstDownloaded = true;
		DataReport::instance().setPreview(DataReport::instance().txf_gettickcount());
		std::string whiteboardReport = DataReport::instance().getWhiteboardReport();
		HttpReportRequest::instance().reportELK(whiteboardReport);
	}
}

void BoardService::onReportBoardData(const int code, const char * msg)
{
}

BoardService::BoardService()
{
	_pagesId.emplace_back("#DEFAULT");
	_backsUrl.emplace_back(L"");
	_backsSend.emplace_back(false);

	TIMManager::instance()->setRecvWBDataCallBack(this);
}


BoardService::~BoardService()
{
	TIMManager::instance()->setRecvWBDataCallBack(nullptr);

	if (_board)
	{
		delete _board;
		_board = nullptr;
	}
}
