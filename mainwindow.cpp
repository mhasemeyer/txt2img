#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QTextDocument>
#include <QTextBlock>
#include <QTextFragment>
#include <qtextlayout.h>
#include <QList>
#include <QMutex>
#include <QThread>
#include <QCustomEvent>
#include <QPainter>
#include <QTextDocumentFragment>
#include <cmath>

#include <QClipboard>
#include <QFileDialog>

#include <QFile>
#include <QTextStream>
#include <QImageWriter>
#include <QMessageBox>
#include <QTextCodec>
#include <QTextDocumentWriter> 

const unsigned MainWindow::imageDPIs[] = {96,300,600};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
	log_(std::cout),
    ui(new Ui::MainWindow),
	folder("."),
	textAlignGroup(this),
	pixmapSize(1024,768),
	pixmapBuf(pixmapSize),
	boxBuilder(log_),
	imageScaleFactor(1.)
{
	ui->setupUi(this);
	QApplication::addLibraryPath("./imageformats");
	//ui->label->setPixmap(pixmap);//,Qt::ColorOnly | Qt::ThresholdDither | Qt::ThresholdAlphaDither));	
	ui->scrollArea->widget()->resize(pixmapSize);
	ui->textEdit->setText("text pattern");	
	setupActions();
	//init();
}

void MainWindow::setupActions()
{
	ui->toolBar->insertWidget(ui->action_bold,ui->fontComboBox);
	ui->toolBar->insertSeparator(ui->action_bold);
	ui->toolBar->insertWidget(ui->action_bold,ui->fontSizeComboBox);
	ui->toolBar->insertSeparator(ui->action_bold);
	ui->toolBar->insertWidget(ui->action_bold,ui->letterSpacingSpinBox);
	ui->toolBar->insertSeparator(ui->action_bold);
	

	QFontDatabase db;
	foreach(int size,db.standardSizes())
		ui->fontSizeComboBox->addItem(QString::number(size));
	
	
	textAlignGroup.addAction(ui->action_justify);
	textAlignGroup.addAction(ui->action_left);
	textAlignGroup.addAction(ui->action_right);
	textAlignGroup.addAction(ui->action_center);

	connect(&textAlignGroup,SIGNAL(triggered(QAction*)),this,SLOT(textAlignChanged(QAction*)));

	ui->action_copy->setEnabled(false);
	ui->action_cut->setEnabled(false);

	connect(ui->action_copy,SIGNAL( triggered()),ui->textEdit,SLOT(copy()));
	connect(ui->action_paste,SIGNAL( triggered()),ui->textEdit,SLOT(paste()));
	connect(ui->action_cut,SIGNAL( triggered()),ui->textEdit,SLOT(cut()));

	connect(QApplication::clipboard(),SIGNAL(dataChanged()),this,SLOT(clipboardDataChanged()));
	connect(ui->textEdit, SIGNAL(copyAvailable(bool)), ui->action_cut, SLOT(setEnabled(bool)));
    connect(ui->textEdit, SIGNAL(copyAvailable(bool)), ui->action_copy, SLOT(setEnabled(bool)));


	
  //   connect(ui->textEdit->document(), SIGNAL(undoAvailable(bool)),
  //           ui->action_undo, SLOT(setEnabled(bool)));
  //   connect(ui->textEdit->document(), SIGNAL(redoAvailable(bool)),
  //           ui->action_redo, SLOT(setEnabled(bool)));

	 //connect(ui->action_undo, SIGNAL(triggered()), ui->textEdit, SLOT(undo()));
  //   connect(ui->action_redo, SIGNAL(triggered()), ui->textEdit, SLOT(redo()));


	 //ui->action_undo->setEnabled(ui->textEdit->document()->isUndoAvailable());
  //   ui->action_redo->setEnabled(ui->textEdit->document()->isRedoAvailable());  

	connect(ui->textEdit->document(),SIGNAL(modificationChanged(bool)),this,SLOT(textDocModificationChanged(bool)));
	 
	 int fontSize = ui->textEdit->currentFont().pointSize();
	 ui->fontSizeComboBox->setEditText(QString::number(fontSize));
	 updateOutFilename();

	QList<QByteArray> formats = QImageWriter::supportedImageFormats();
	QComboBox *formatsCombo = ui->imgFormatcomboBox;
	std::for_each(formats.begin(),formats.end(),[formatsCombo](const QByteArray& ba)
	{
		formatsCombo->addItem(ba);		
		if(ba=="png")
			formatsCombo->setCurrentIndex(formatsCombo->count()-1);
	});

	for(size_t i = 0; i < 3; ++i)
	{
		ui->imageDPIComboBox->addItem(QString::number(imageDPIs[i]));
	}

	updateWindowTitle();
	charMapper = CharMapper("./charmap.txt");
}

MainWindow::~MainWindow()
{
    delete ui;

}


void MainWindow::scaleImage(double scaleFactor)
{
	//ui->label->resize(scaleFactor*pixmap.size());
	ui->scrollArea->widget()->resize(scaleFactor*pixmapSize);
	pixmapBuf = QPixmap(ui->scrollArea->widget()->size());
	showImage();
	ui->action_zoomin->setEnabled(imageScaleFactor < 4.);
	ui->action_zoomout->setEnabled(imageScaleFactor > .025);
}

void MainWindow::on_fontComboBox_currentFontChanged(const QFont &font)
{
	ui->textEdit->setCurrentFont(font);
	
}
void MainWindow::textBoxFontChanged(const QFont& font)
{	
	ui->fontComboBox->setCurrentIndex(ui->fontComboBox->findText(QFontInfo(font).family()));
	ui->fontSizeComboBox->setCurrentIndex(ui->fontSizeComboBox->findText(QString::number(font.pointSize())));
	ui->action_bold->setChecked(font.bold());
	ui->action_italic->setChecked(font.italic());
	ui->action_underline->setChecked(font.underline());
	QFont::StyleStrategy ss = font.styleStrategy();
	bool checked = (ss & QFont::PreferAntialias && !(ss & QFont::NoAntialias));
	ui->checkBox->setChecked(checked);
	ui->letterSpacingSpinBox->setValue(font.letterSpacing()==0? 1: font.letterSpacing()/100);
	updateOutFilename();
}


void MainWindow::on_imgSizeSetButton_clicked()
{
	bool ok;
	int width = ui->imgXEdit->text().toInt(&ok);
	if(!ok) return;
	int height = ui->imgYEdit->text().toInt(&ok);
	if(!ok) return;
	pixmapSize.setWidth(width);
	pixmapSize.setHeight(height);
	boxBuilder.clearBoxes();
	scaleImage(1);
}

void MainWindow::textBold()
{
	QTextCharFormat format;
	format.setFontWeight(ui->action_bold->isChecked() ? QFont::Bold : QFont::Normal);
	mergeFormatOnWordOrSelection(format);
}

void MainWindow::textItalic()
{
	QTextCharFormat format;
	format.setFontItalic(ui->action_italic->isChecked());
	mergeFormatOnWordOrSelection(format);
}

void MainWindow::textUnderline()
{
	QTextCharFormat format;
	format.setFontUnderline(ui->action_underline->isChecked());
	mergeFormatOnWordOrSelection(format);
}

void MainWindow::textSize(QString size)
{
	bool ok;
	qreal pointSize = size.toFloat(&ok);
	if(ok)
	{
		QTextCharFormat format = ui->textEdit->currentCharFormat();
		format.setFontPointSize(pointSize);
		mergeFormatOnWordOrSelection(format);
	}
}

void MainWindow::showImage()
{
	pixmapBuf.fill(Qt::white);
	QPainter painter(&pixmapBuf);
	painter.setPen(Qt::red);
	QPixmap pm = boxBuilder.pixmap().scaled(pixmapBuf.size());
	painter.drawPixmap(0,0,pm);


	//const std::list<BoxBuilder::box>& boxes = boxBuilder.boxes();	
	const std::list<BoxBuilder::box>& boxes = ui->useCharMappingCheckBox->isChecked()? charMapper.boxes() : boxBuilder.boxes();

	for(std::list<BoxBuilder::box>::const_iterator boxIt = boxes.begin(); boxIt != boxes.end(); ++boxIt)
	{
		double scaleFactor = static_cast<double>(pixmapBuf.size().width())/boxBuilder.pixmap().size().width();
		QRect boundingRect = boxIt->boundingRect;
		QRect scaledRect(	scaleFactor*boundingRect.x(),
							scaleFactor*(boxBuilder.pixmap().height() - boundingRect.y() - boundingRect.height()),
							scaleFactor*boundingRect.width(),
							scaleFactor*boundingRect.height());

		//log_ << "scaledRect = (" << scaledRect.x() << "," << scaledRect.y() <<
		//		"," << scaledRect.width() << "," << scaledRect.height() << ")" << std::endl;
		if(!ui->useCharMappingCheckBox->isChecked())
		{
			double histValue = static_cast<double>(boxBuilder.histValue(boxIt->character))/boxBuilder.maxHistValue();
			if(histValue < 0)
				painter.setPen(Qt::black);
			else if(histValue < .25)
				painter.setPen(Qt::yellow);
			else if(histValue <.5)
				painter.setPen(Qt::cyan);
			else if(histValue < .75)
				painter.setPen(Qt::blue);
			else painter.setPen(Qt::green);
		}
		else painter.setPen(Qt::red);
		
		painter.drawRect(scaledRect);
	}
	
	ui->label->setPixmap(pixmapBuf);
}

void MainWindow::on_genImgButton_clicked()
{
	QTextDocument* doc = ui->textEdit->document();	
	QSizeF sz = doc->size();
	if(sz.width() > this->pixmapSize.width() || sz.height() > pixmapSize.height())
	{
		int ret = QMessageBox::warning(this,"attention",
								"The size of the document exceeds the size of the image, increase image?",
		QMessageBox::Yes,QMessageBox::No);
		if(ret == QMessageBox::Yes)
		{
			ui->imgXEdit->setText(QString::number(static_cast<int>(sz.width())));
			ui->imgYEdit->setText(QString::number(static_cast<int>(sz.height())));
			on_imgSizeSetButton_clicked();
		}
	}

	boxBuilder.build(doc,pixmapSize);
	
	
	charMapper.mapBoxes(boxBuilder.boxes());	
	showImage();
}

void MainWindow::mergeFormatOnWordOrSelection(const QTextCharFormat& format)
{
	QTextCursor cursor = ui->textEdit->textCursor();
	if(!cursor.hasSelection())
		cursor.select(QTextCursor::WordUnderCursor);
	cursor.mergeCharFormat(format);
	ui->textEdit->mergeCurrentCharFormat(format);
}
void MainWindow::on_textEdit_cursorPositionChanged()
{
	alignmentChanged(ui->textEdit->alignment());
}

void MainWindow::on_textEdit_currentCharFormatChanged(const QTextCharFormat &format)
{
	textBoxFontChanged(format.font());
}
void MainWindow::textAlignChanged(QAction* action)
{
	if (action == ui->action_left)
         ui->textEdit->setAlignment(Qt::AlignLeft | Qt::AlignAbsolute);
	else if (action == ui->action_center)
         ui->textEdit->setAlignment(Qt::AlignHCenter);
	else if (action == ui->action_right)
         ui->textEdit->setAlignment(Qt::AlignRight | Qt::AlignAbsolute);
	else if (action == ui->action_justify)
         ui->textEdit->setAlignment(Qt::AlignJustify);
}

void MainWindow::alignmentChanged(Qt::Alignment a)
{
	 if (a & Qt::AlignLeft)
		 ui->action_left->setChecked(true);
     else if (a & Qt::AlignHCenter)
		 ui->action_center->setChecked(true);
     else if (a & Qt::AlignRight)
		 ui->action_right->setChecked(true);
     else if (a & Qt::AlignJustify)
		 ui->action_justify->setChecked(true);     
}


void MainWindow::clipboardDataChanged()
{
	if (const QMimeData *md = QApplication::clipboard()->mimeData())
         ui->action_paste->setEnabled(md->hasText());
}


void MainWindow::zoomin()
{
	imageScaleFactor *= 1.25;
	scaleImage(imageScaleFactor);
	
}

void MainWindow::zoomout()
{
	imageScaleFactor *= 0.75;
	scaleImage(imageScaleFactor);	
}

void MainWindow::cancelZoom()
{
	imageScaleFactor = 1.;
	scaleImage(imageScaleFactor);
}
void MainWindow::on_checkBox_toggled(bool checked)
{
	QFont font = ui->textEdit->currentFont();
	QFont::StyleStrategy ss = font.styleStrategy();	
	if(checked)
	{	
		ss = static_cast<QFont::StyleStrategy>(ss & ~QFont::NoAntialias);		
		ss = static_cast<QFont::StyleStrategy>(ss | QFont::PreferAntialias);	
	}
	else 
	{
		ss = static_cast<QFont::StyleStrategy>(ss & ~QFont::PreferAntialias);
		ss = static_cast<QFont::StyleStrategy>(ss | QFont::NoAntialias);		
	}

	font.setStyleStrategy(ss);	
	ui->textEdit->setCurrentFont(font);
	ui->textEdit->update();
}

void MainWindow::saveBoxAndImage()
{	
	QFile boxLineFormat("./config.txt");
	if(!boxLineFormat.open(QFile::ReadOnly|QFile::Text))
	{
		QMessageBox::critical(this,"error","cannot open box line format config file");
		return;

	}
	QTextStream boxLineFormatStream(&boxLineFormat);
	boxLineFormatStream.setCodec(QTextCodec::codecForName("UTF-8"));
	QString formatLine = boxLineFormatStream.readLine();
	
	QString boxFileName = QString(folder + "/" + ui->outFilenameLineEdit->text() + ".box");
	boxFileName.replace(QRegExp("\\s+"),".");
	QFile boxOut(boxFileName);
	if(!boxOut.open(QFile::WriteOnly|QFile::Text))
	{
		QMessageBox::critical(this,"error","cannot open box output file for write");
		return;
	}
	
	QTextStream boxStream(&boxOut);
	
	boxStream.setCodec(QTextCodec::codecForName("UTF-8"));


	//const std::list<BoxBuilder::box>& boxes = boxBuilder.boxes();
	
	const std::list<BoxBuilder::box>& boxes = ui->useCharMappingCheckBox->isChecked()? charMapper.boxes() : boxBuilder.boxes();
	
	std::for_each(boxes.begin(),boxes.end(),[&boxStream,&formatLine](const BoxBuilder::box& b)
	{
		QString left = QString::number(b.boundingRect.left());
		QString top = QString::number(b.boundingRect.top());
		QString right = QString::number(b.boundingRect.right());
		QString bottom = QString::number(b.boundingRect.bottom());

		QString width = QString::number(b.boundingRect.width());
		QString height = QString::number(b.boundingRect.height());
		QString boxOutLine = formatLine;
		
		
		boxOutLine.replace(QString("$CHAR"),b.character);
		
		boxOutLine.replace(QString("$LEFT"),left);
		boxOutLine.replace(QString("$TOP"),top);

		boxOutLine.replace(QString("$RIGHT"),right);
		boxOutLine.replace(QString("$BOTTOM"),bottom);

		boxOutLine.replace(QString("$WIDTH"),width);
		boxOutLine.replace(QString("$HEIGHT"),height);
		//wchar_t uc = b.character.unicode();
		//boxStream << b.character << QString::fromUtf16(L"\n");
		//std::cout << "vvvv" << std::endl;
		boxStream << boxOutLine << QString::fromUtf8("\n");
	});
	boxOut.close();
	QImage imageToWrite = boxBuilder.pixmap().toImage();
	qreal dpm = 1000./25.4*ui->imageDPIComboBox->currentText().toInt();
	imageToWrite.setDotsPerMeterX(dpm);
	imageToWrite.setDotsPerMeterY(dpm);	
	//if(!boxBuilder.pixmap().save(folder + "/" + ui->outFilenameLineEdit->text() + "." + ui->imgFormatcomboBox->currentText()))
	QString imageFileName = folder + "/" + ui->outFilenameLineEdit->text() + "." + ui->imgFormatcomboBox->currentText();
	imageFileName.replace(QRegExp("\\s+"),".");
	if(!imageToWrite.save(imageFileName))
	{
		QList<QByteArray> formats = QImageWriter::supportedImageFormats();
		QString supportedFormats;
		std::for_each(formats.begin(),formats.end(),[&supportedFormats](const QByteArray& ba)
		{
			supportedFormats += ba + ",";
		});
		supportedFormats.replace(QRegExp(",$"),"");
		QMessageBox::critical(this,"error","couldn't write image file, perhaps the format is not supported. Currently supported: " + supportedFormats);
		return;
	}
	QMessageBox::information(this,"info","image and box files are saved to " + folder);

}

void MainWindow::updateOutFilename()
{
	ui->outFilenameLineEdit->setText(QString("eng.") + ui->textEdit->currentFont().family() + ".exp0");	
}
void MainWindow::on_selectFolderButton_clicked()
{
	folder = QFileDialog::getExistingDirectory();
	ui->folderLabel_->setText(folder);
}

void MainWindow::on_letterSpacingSpinBox_valueChanged(double arg1)
{		
	QTextCharFormat format = ui->textEdit->currentCharFormat();;
	QFont font = format.font();
	font.setLetterSpacing(QFont::PercentageSpacing,arg1*100);
	format.setFont(font);
	mergeFormatOnWordOrSelection(format);
}


void MainWindow::textOpen()
{
	QString fileName = QFileDialog::getOpenFileName(this,"select a file to open",".","HTML File (*.html);; Text File (*.txt)");
	if(fileName.length()==0) return;
	textDocFileInfo.setFile(fileName);
	QFile inputFile(fileName);
	if(!inputFile.open(QIODevice::ReadOnly|QIODevice::Text))
	{
		QMessageBox::critical(this,"error","cannot read " + fileName);
		return;
	}
	QTextStream html(&inputFile);
	QString htmlContents = html.readAll();
	ui->textEdit->clear();
	
	if(fileName.endsWith(".html"))
		ui->textEdit->insertHtml(htmlContents);
	else if(fileName.endsWith(".txt"))
		ui->textEdit->insertPlainText(htmlContents);
	else
	{
		QMessageBox::critical(this,"unsupported file","cannot open " + fileName);
		return;
	}
	
	ui->textEdit->document()->setModified(false);
	
}

void MainWindow::textNew()
{
	textDocFileInfo = QFileInfo();
	ui->textEdit->clear();	
	updateWindowTitle();
}

void MainWindow::textSaveAs()
{
	QString fileName = QFileDialog::getSaveFileName(this,"select a file name to save text",".","HTML files (*html)");
	if(fileName.length()==0) return;
	if(!fileName.endsWith(".html"))
		fileName += ".html";
	
	textDocFileInfo.setFile(fileName);
	saveTextDoc(fileName);	
	updateWindowTitle();
}

void MainWindow::textSave()
{
	if(textDocFileInfo.fileName().length()==0)
		return textSaveAs();
	
	saveTextDoc(textDocFileInfo.absoluteFilePath());
}


void MainWindow::saveTextDoc(const QString& fileName)
{
	QTextDocumentWriter writer(fileName,"HTML");
	writer.setCodec(QTextCodec::codecForName("UTF-8"));
	writer.write(ui->textEdit->document());
	ui->textEdit->document()->setModified(false);
}

void MainWindow::textDocModificationChanged(bool changed)
{
	if(changed)
		setWindowTitle(getWindowTitle() + "*");
	else
		setWindowTitle(getWindowTitle());
}


QString MainWindow::getWindowTitle() const
{
	QString windowTitle = "txt2img :: ";
	QString fileName = textDocFileInfo.fileName();
	windowTitle += fileName.length()==0 ? "untitled" : fileName;
	return windowTitle;
}

void MainWindow::updateWindowTitle()
{	
	setWindowTitle(getWindowTitle());
}
void MainWindow::on_selectCharMapButton_clicked()
{
	QString fileName = QFileDialog::getOpenFileName(this,"select the char mapping file to open",".","text file (*.txt)");
	if(fileName.length()==0) return;	
	try
	{
		charMapper = CharMapper(fileName);
	}
	catch(std::runtime_error& err)
	{
		QMessageBox::critical(this,"error parsing char mapping file",QString(err.what()));
		return;
	}
	//QFileInfo fileInfo(fileName);
	ui->charMapFileNameEdit->setText(fileName);
}

void MainWindow::on_useCharMappingCheckBox_toggled(bool checked)
{	
	ui->charMapFileNameEdit->setEnabled(checked);
	ui->selectCharMapButton->setEnabled(checked);
}
