// -*- Mode: Go; indent-tabs-mode: t -*-

/*
 * Copyright (C) 2018 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

package daemon

import (
	"context"
	"encoding/json"
	"net/http"

	"gopkg.in/check.v1"

	"github.com/snapcore/snapd/client"
	"github.com/snapcore/snapd/overlord/auth"
	"github.com/snapcore/snapd/overlord/state"
)

func MockSnapshotSave(newSave func(*state.State, []string, []string) (uint64, []string, *state.TaskSet, error)) (restore func()) {
	oldSave := snapshotSave
	snapshotSave = newSave
	return func() {
		snapshotSave = oldSave
	}
}

func MockSnapshotList(newList func(context.Context, uint64, []string) ([]client.SnapshotSet, error)) (restore func()) {
	oldList := snapshotList
	snapshotList = newList
	return func() {
		snapshotList = oldList
	}
}

func MockSnapshotCheck(newCheck func(*state.State, uint64, []string, []string) ([]string, *state.TaskSet, error)) (restore func()) {
	oldCheck := snapshotCheck
	snapshotCheck = newCheck
	return func() {
		snapshotCheck = oldCheck
	}
}

func MockSnapshotRestore(newRestore func(*state.State, uint64, []string, []string) ([]string, *state.TaskSet, error)) (restore func()) {
	oldRestore := snapshotRestore
	snapshotRestore = newRestore
	return func() {
		snapshotRestore = oldRestore
	}
}

func MockSnapshotForget(newForget func(*state.State, uint64, []string) ([]string, *state.TaskSet, error)) (restore func()) {
	oldForget := snapshotForget
	snapshotForget = newForget
	return func() {
		snapshotForget = oldForget
	}
}

func MustUnmarshalSnapInstruction(c *check.C, jinst string) *snapInstruction {
	var inst snapInstruction
	if err := json.Unmarshal([]byte(jinst), &inst); err != nil {
		c.Fatalf("cannot unmarshal %q into snapInstruction: %v", jinst, err)
	}
	return &inst
}

func MustUnmarshalSnapshotAction(c *check.C, jact string) *snapshotAction {
	var act snapshotAction
	if err := json.Unmarshal([]byte(jact), &act); err != nil {
		c.Fatalf("cannot unmarshal %q into snapshotAction: %v", jact, err)
	}
	return &act
}

func (rsp *resp) ErrorResult() *errorResult {
	return rsp.Result.(*errorResult)
}

func ListSnapshots(c *Command, r *http.Request, user *auth.UserState) *resp {
	return listSnapshots(c, r, user).(*resp)
}

func ChangeSnapshots(c *Command, r *http.Request, user *auth.UserState) *resp {
	return changeSnapshots(c, r, user).(*resp)
}

var (
	SnapshotMany = snapshotMany
	SnapshotCmd  = snapshotCmd
)
