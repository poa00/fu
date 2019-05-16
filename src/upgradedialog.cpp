#include "upgradedialog.h"
#include "ui_upgradedialog.h"

UpgradeDialog::UpgradeDialog() :
    QDialog(),
    ui(new Ui::UpgradeDialog)
{
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose);
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(startUpgrading()));
}

void UpgradeDialog::setMigrator(Migrator *m)
{
    migrator = m;
    progressUpdate(1, 0);
}

UpgradeDialog::~UpgradeDialog()
{
    delete ui;
}

void UpgradeDialog::startUpgrading()
{
    connect(migrator, SIGNAL(progressChanged(int,  double)), this, SLOT(progressUpdate(int, double)));
    connect(migrator, SIGNAL(finished()), this, SLOT(accept()));
    migrator->run();
    ui->buttonBox->setEnabled(false);
}

void UpgradeDialog::progressUpdate(int step, double percent)
{
    ui->lblStep->setText(QString("%1 : %2/%3").arg(tr("Step")).arg(step).arg(migrator->totalPendingMigration()));
    ui->prgPercentage->setValue(static_cast<int>(percent*100));
}